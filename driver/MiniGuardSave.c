#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>

#include "utils.h"
#pragma prefast(disable : __WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")
#define MAX_PARENT_PIDS 50

typedef struct _PRE_2_POST_CONTEXT
{

    HANDLE fileHandle;

} PRE_2_POST_CONTEXT, *PPRE_2_POST_CONTEXT;

int pids[MAX_PARENT_PIDS] = {0};
PFLT_FILTER FilterHandle = NULL;
PFLT_PORT port = NULL;
PFLT_PORT ClientPort = NULL;
PWCHAR rootDir = L"\\Device\\HarddiskVolume2\\Users\\WDAGUtilityAccount\\Desktop\\folder\\";
void PcreateProcessNotifyRoutine(
    HANDLE ParentId,
    HANDLE ProcessId,
    BOOLEAN Create)
{

    int index = findFirstsZero(pids, MAX_PARENT_PIDS);
    if (index != -1)
    {
        for (int i = 0; i < MAX_PARENT_PIDS; i++)
        {

            if (pids[i] == HandleToLong(ParentId))
            {
                pids[index] = HandleToLong(ProcessId);
                KdPrint(("parent: %lu,process: %lu,create: %s\n", HandleToLong(ParentId), HandleToLong(ProcessId), Create ? "TRUE" : "FALSE"));
            }
        }
    }
}

NTSTATUS OnConnect(
    PFLT_PORT clientPort,
    PVOID ServerPortCookie,
    PVOID ConnectionContext,
    ULONG SizeOfContext,
    PVOID ConnectionPortCookie)
{
    UNREFERENCED_PARAMETER(ConnectionPortCookie);
    UNREFERENCED_PARAMETER(SizeOfContext);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(ServerPortCookie);

    ClientPort = clientPort;
    KdPrint(("MiniGuard connected\r\n"));
    return STATUS_SUCCESS;
};

VOID OnDisconnect(
    PVOID ConnectionCookie)
{
    UNREFERENCED_PARAMETER(ConnectionCookie);

    FltCloseClientPort(FilterHandle, &ClientPort);
    KdPrint(("MiniGuard disconnected\r\n"));
};
typedef struct _MESSAGE
{
    ULONG command;
    PCWSTR value;
} MESSAGE, *PMESSAGE;
NTSTATUS OnCommunication(

    PVOID PortCookie,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength,
    PULONG ReturnOutputBufferLength)
{
    UNREFERENCED_PARAMETER(PortCookie);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(ReturnOutputBufferLength);
    PMESSAGE FltMessage = (PMESSAGE)InputBuffer;
    UNICODE_STRING unicodeString;
    RtlInitUnicodeString(&unicodeString, FltMessage->value);
    KdPrint(("Type: %d,Value: %wZ\r\n", FltMessage->command, &unicodeString));
    ULONG value = 0;
    NTSTATUS status = RtlUnicodeStringToInteger(&unicodeString, 10, &value);
    if (NT_SUCCESS(status))
    {
        pids[0] = (int)value;
        for (int j = 0; j < MAX_PARENT_PIDS; j++)
        {
            KdPrint(("%d ", pids[j]));
        }
        KdPrint(("\n"));
    }

    return STATUS_SUCCESS;
};

NTSTATUS
MiniGuardUnload(
    FLT_FILTER_UNLOAD_FLAGS Flags)

{
    UNREFERENCED_PARAMETER(Flags);

    FltCloseCommunicationPort(port);
    FltUnregisterFilter(FilterHandle);
    KdPrint(("MiniGuard unloaded successfully\r\n"));
    PsSetCreateProcessNotifyRoutine(PcreateProcessNotifyRoutine, TRUE);
    return STATUS_SUCCESS;
}
FLT_PREOP_CALLBACK_STATUS
MiniGuardPreWriteOperation(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID *CompletionContext)
{

    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    PUNICODE_STRING fullPath = getFullPath(Data);

    ULONG process = FltGetRequestorProcessId(Data);
    if (isInArray(pids, MAX_PARENT_PIDS, process) == 0 || !FLT_IS_IRP_OPERATION(Data))
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }
    if (wcsncmp(fullPath->Buffer, rootDir, wcslen(rootDir)) == 0)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }
    // KdPrint(("WRITING FULL PATH: %wZ\r\n", fullPath));
    if (Data->Iopb->Parameters.Write.WriteBuffer != NULL)
    {
        NTSTATUS status;
        OBJECT_ATTRIBUTES objAttr;
        IO_STATUS_BLOCK ioStatusBlock;
        HANDLE fileHandle;
        PFILE_OBJECT fileObject;
        UNICODE_STRING URootDir;

        RtlInitUnicodeString(&URootDir, rootDir);
        PUNICODE_STRING fullPath2 = (PUNICODE_STRING)ExAllocatePool(NonPagedPoolNx, sizeof(UNICODE_STRING));
        if (fullPath2 == NULL)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        fullPath2->MaximumLength = (USHORT)(fullPath->MaximumLength + sizeof(WCHAR) * 2);
        fullPath2->Buffer = (WCHAR *)ExAllocatePool2(POOL_FLAG_PAGED, fullPath->MaximumLength, 'MGT1');
        fullPath2->Length = 0;

        RtlCopyUnicodeString(fullPath2, fullPath);
        UNICODE_STRING newFilePath = ReplaceDeviceName(fullPath2, getDeviceName(Data->Iopb->TargetFileObject->DeviceObject), &URootDir);
        CreateDirectory(FilterHandle, FltObjects->Instance, newFilePath.Buffer, TRUE);
        if (!NT_SUCCESS(FileExists(FilterHandle, FltObjects->Instance, newFilePath.Buffer)))
        {

            copyFile(FilterHandle, FltObjects->Instance, Data->Iopb->TargetFileObject, &newFilePath);
        }

        InitializeObjectAttributes(&objAttr, &newFilePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
        status = FltCreateFileEx(FltObjects->Filter,                                     /* Filter            */
                                 FltObjects->Instance,                                   /* Instance          */
                                 &fileHandle,                                            /* FileHandle        */
                                 &fileObject,                                            /* FileObject        */
                                 FILE_GENERIC_WRITE | FILE_GENERIC_READ,                 /* DesiredAccess     */
                                 &objAttr,                                               /* ObjectAttributes  */
                                 &ioStatusBlock,                                         /* IoStatusBlock     */
                                 NULL,                                                   /* AllocationSize    */
                                 FILE_ATTRIBUTE_NORMAL,                                  /* FileAttributes    */
                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, /* ShareAccess       */
                                 FILE_OVERWRITE_IF,                                      /* CreateDisposition */
                                 FILE_SYNCHRONOUS_IO_NONALERT,                           /* CreateOptions     */
                                 NULL,                                                   /* EaBuffer          */
                                 0,                                                      /* EaLength          */
                                 IO_FORCE_ACCESS_CHECK);

        // KdPrint(("FULL PATH: %wZ\r\n", fullPath));

        if (!NT_SUCCESS(status))
        {
            KdPrint(("PATH could not be opened: %wZ, status: %x\r\n", fullPath, status));
            FltClose(fileHandle);
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        Data->Iopb->TargetFileObject = fileObject;
        FltSetCallbackDataDirty(Data);

        PPRE_2_POST_CONTEXT p2pCtx = (PPRE_2_POST_CONTEXT)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(PRE_2_POST_CONTEXT), 'p2pC');
        if (p2pCtx == NULL)
        {
            FltClose(fileHandle);
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        p2pCtx->fileHandle = fileHandle;
        *CompletionContext = p2pCtx;
    }
    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
MiniGuardPostWriteOperation(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID CompletionContext,
    FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    PPRE_2_POST_CONTEXT p2pCtx = CompletionContext;

    if (p2pCtx == NULL)
    {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FltClose(p2pCtx->fileHandle);
    ExFreePool(p2pCtx);
    return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS
MiniGuardPreOpenOperation2(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID *CompletionContext)
{

    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    ULONG process = FltGetRequestorProcessId(Data);
    if (isInArray(pids, MAX_PARENT_PIDS, process) == 0)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    PUNICODE_STRING fullPath = getFullPath(Data);

    if (wcsncmp(fullPath->Buffer, rootDir, wcslen(rootDir)) != 0)
    {
        PUNICODE_STRING deviceName = getDeviceName(Data->Iopb->TargetFileObject->DeviceObject);
        UNICODE_STRING newFilePath;
        UNICODE_STRING URootDir;
        RtlInitUnicodeString(&URootDir, rootDir);
        RtlInitUnicodeString(&newFilePath, ReplaceDeviceName(fullPath, deviceName, &URootDir).Buffer);
        if (NT_SUCCESS(FileExists(FilterHandle, FltObjects->Instance, newFilePath.Buffer)))
        {
            IoReplaceFileObjectName(Data->Iopb->TargetFileObject, newFilePath.Buffer, newFilePath.Length);
            Data->Iopb->TargetFileObject->RelatedFileObject = NULL;
            Data->IoStatus.Information = IO_REPARSE;
            Data->IoStatus.Status = STATUS_REPARSE;
            FltSetCallbackDataDirty(Data);
            return FLT_PREOP_COMPLETE;
        }
    }
    // KdPrint(("FULL PATH: %wZ\r\n", fullPath));

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS
MiniGuardPreOpenOperation(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID *CompletionContext)
{

    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    ULONG process = FltGetRequestorProcessId(Data);
    if (isInArray(pids, MAX_PARENT_PIDS, process) == 0)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    PUNICODE_STRING deviceName = getDeviceName(Data->Iopb->TargetFileObject->DeviceObject);
    UNICODE_STRING fileName = Data->Iopb->TargetFileObject->FileName;
    UNICODE_STRING fullPath;
    fullPath.Buffer = (WCHAR *)ExAllocatePool2(POOL_FLAG_PAGED, deviceName->MaximumLength + fileName.MaximumLength + sizeof(WCHAR) * 2, 'MGT1');
    fullPath.Length = 0;
    fullPath.MaximumLength = (USHORT)(deviceName->MaximumLength + fileName.MaximumLength + sizeof(WCHAR) * 2);
    RtlAppendUnicodeStringToString(&fullPath, deviceName);
    RtlAppendUnicodeStringToString(&fullPath, &fileName);
    // KdPrint(("Full PATH: %wZ\r\n", &fullPath));

    if (wcsncmp(fullPath.Buffer, rootDir, wcslen(rootDir)) != 0)
    {
        if (Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess & FILE_EXECUTE)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        if (
            !(Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess &
              (DELETE | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_SUPERSEDED | FILE_CREATE | FILE_OVERWRITE)) &&
            (!(Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess & (FILE_OVERWRITE_IF | FILE_OPEN_IF)) ||
             NT_SUCCESS(FileExists(FilterHandle, FltObjects->Instance, fullPath.Buffer)))

        )
        {
            UNICODE_STRING newFilePath;
            UNICODE_STRING URootDir;
            RtlInitUnicodeString(&URootDir, rootDir);
            RtlInitUnicodeString(&newFilePath, ReplaceDeviceName(&fullPath, deviceName, &URootDir).Buffer);
            if (NT_SUCCESS(FileExists(FilterHandle, FltObjects->Instance, newFilePath.Buffer)))
            {
                IoReplaceFileObjectName(Data->Iopb->TargetFileObject, newFilePath.Buffer, newFilePath.Length);
                Data->Iopb->TargetFileObject->RelatedFileObject = NULL;
                Data->IoStatus.Information = IO_REPARSE;
                Data->IoStatus.Status = STATUS_REPARSE;
                FltSetCallbackDataDirty(Data);
                return FLT_PREOP_COMPLETE;
            }

            KdPrint(("Operation: inside \r\n"));
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        UNICODE_STRING newFilePath;
        UNICODE_STRING URootDir;
        RtlInitUnicodeString(&URootDir, rootDir);
        RtlInitUnicodeString(&newFilePath, ReplaceDeviceName(&fullPath, deviceName, &URootDir).Buffer);
        CreateDirectory(FilterHandle, FltObjects->Instance, newFilePath.Buffer, TRUE);
        IoReplaceFileObjectName(Data->Iopb->TargetFileObject, newFilePath.Buffer, newFilePath.Length);
        Data->Iopb->TargetFileObject->RelatedFileObject = NULL;
        Data->IoStatus.Information = IO_REPARSE;
        Data->IoStatus.Status = STATUS_REPARSE;
        FltSetCallbackDataDirty(Data);
        return FLT_PREOP_COMPLETE;
    }

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
};

FLT_PREOP_CALLBACK_STATUS
MiniGuardPreRenameOperation(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID *CompletionContext)
{

    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    // NTSTATUS status;
    ULONG process = FltGetRequestorProcessId(Data);
    if (isInArray(pids, MAX_PARENT_PIDS, process) == 0)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }
    PUNICODE_STRING fullPath = getFullPath(Data);

    // PUNICODE_STRING fullPath = getFullPath(Data);
    switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass)
    {
    case FileEndOfFileInformation:
        KdPrint(("FileEndOfFileInformation\r\n"));
        return FLT_PREOP_COMPLETE;
    case FileDispositionInformation:
    case FileDispositionInformationEx:
        KdPrint(("FileDispositionInformation\r\n"));
        return FLT_PREOP_COMPLETE;
    case FileRenameInformation:
    case FileRenameInformationEx:

        PFILE_RENAME_INFORMATION renameInfo = (PFILE_RENAME_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;

        PFLT_FILE_NAME_INFORMATION DestinationFileNameInformation = NULL;
        FltGetDestinationFileNameInformation(
            FltObjects->Instance,
            FltObjects->FileObject,
            renameInfo->RootDirectory,
            renameInfo->FileName,
            renameInfo->FileNameLength,
            FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
            &DestinationFileNameInformation);

        UNICODE_STRING URootDir;
        RtlInitUnicodeString(&URootDir, rootDir);

        UNICODE_STRING newFilePath = ReplaceDeviceName(&DestinationFileNameInformation->Name, getDeviceName(Data->Iopb->TargetFileObject->DeviceObject), &URootDir);

        if (wcsncmp(fullPath->Buffer, rootDir, wcslen(rootDir)) != 0 &&
            !NT_SUCCESS(FileExists(FilterHandle, FltObjects->Instance, newFilePath.Buffer)))

        {

            CreateDirectory(FilterHandle, FltObjects->Instance, newFilePath.Buffer, TRUE);
            copyFile(FilterHandle, FltObjects->Instance, FltObjects->FileObject, &newFilePath);
            return FLT_PREOP_COMPLETE; // copy and rename outside file if it does not exist in miniguard
        }
        UNICODE_STRING miniGuardPath = getMiniGuardPath(&URootDir);
        if (wcsncmp(fullPath->Buffer, miniGuardPath.Buffer, wcslen(miniGuardPath.Buffer)) == 0)
        {
            // if file exists in miniguard rename it.
            KdPrint(("DestinationFileNameInformation: %wZ\r\n", DestinationFileNameInformation->Name));
            PFILE_RENAME_INFORMATION newRenameInfo = (PFILE_RENAME_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(FILE_RENAME_INFORMATION) + newFilePath.Length + sizeof(WCHAR), 'MGT1');
            if (newRenameInfo == NULL)
            {
                return FLT_PREOP_COMPLETE;
            }
            KdPrint(("NewPath: %wZ\r\n", newFilePath));
            newRenameInfo->RootDirectory = renameInfo->RootDirectory;
            newRenameInfo->FileNameLength = newFilePath.Length;
            newRenameInfo->ReplaceIfExists = TRUE;
            CreateDirectory(FilterHandle, FltObjects->Instance, newFilePath.Buffer, TRUE);
            RtlCopyMemory(newRenameInfo->FileName, newFilePath.Buffer, newFilePath.Length);
            FltSetInformationFile(FltObjects->Instance, FltObjects->FileObject, newRenameInfo, newRenameInfo->FileNameLength, FileRenameInformation);
            ExFreePool(newRenameInfo);
            FltSetCallbackDataDirty(Data);
            FltReleaseFileNameInformation(DestinationFileNameInformation);
            return FLT_PREOP_COMPLETE;
        }

        if (wcsncmp(fullPath->Buffer, rootDir, wcslen(rootDir)) == 0)
        {
            // if file is local do whatever you want
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
    }
    KdPrint(("Unhandled FileInformationClass: %d\r\n", Data->Iopb->Parameters.SetFileInformation.FileInformationClass));
    KdPrint(("RENAME PATH: %wZ\r\n", fullPath));
    if (wcsncmp(fullPath->Buffer, rootDir, wcslen(rootDir)) != 0)
    {

        return FLT_PREOP_COMPLETE;
    }
    else
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }
};

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    {IRP_MJ_SET_INFORMATION,
     0,
     MiniGuardPreRenameOperation,
     NULL},
    {IRP_MJ_CREATE,
     0,
     MiniGuardPreOpenOperation2,
     NULL},

    {IRP_MJ_WRITE,
     0,
     MiniGuardPreWriteOperation,
     MiniGuardPostWriteOperation},
    {IRP_MJ_OPERATION_END}};

VOID CtxContextCleanup(
    _In_ PFLT_CONTEXT Context,
    _In_ FLT_CONTEXT_TYPE ContextType)
{
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(ContextType);
}

FLT_REGISTRATION FilterRegistration = {
    sizeof(FLT_REGISTRATION),
    FLT_REGISTRATION_VERSION,
    0,
    NULL,
    Callbacks,
    MiniGuardUnload,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    NTSTATUS status;
    PSECURITY_DESCRIPTOR sd;
    OBJECT_ATTRIBUTES oa = {0};
    UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\MiniGuard");

    status = FltRegisterFilter(DriverObject, &FilterRegistration, &FilterHandle);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = FltBuildDefaultSecurityDescriptor(
        &sd,
        FLT_PORT_ALL_ACCESS);
    if (!NT_SUCCESS(status))
    {
        FltUnregisterFilter(FilterHandle);
        return status;
    }
    InitializeObjectAttributes(
        &oa,
        &name,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, sd);

    status = PsSetCreateProcessNotifyRoutine(PcreateProcessNotifyRoutine, FALSE);
    if (!NT_SUCCESS(status))
    {
        FltUnregisterFilter(FilterHandle);
        return status;
    }

    FltCreateCommunicationPort(
        FilterHandle,
        &port,
        &oa,
        NULL,
        OnConnect,
        OnDisconnect,
        OnCommunication,
        1);
    status = FltStartFiltering(FilterHandle);
    if (!NT_SUCCESS(status))
    {
        FltUnregisterFilter(FilterHandle);
        return status;
    }

    KdPrint(("MiniGuard loaded successfully\r\n"));
    return status;
}
