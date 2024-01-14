#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>

#include "utils.h"
#pragma prefast(disable : __WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")
#define MAX_PARENT_PIDS 10

typedef struct _PRE_2_POST_CONTEXT
{

    PHANDLE fileHandle;

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

    // ULONG process = FltGetRequestorProcessId(Data);
    // if (isInArray(pids, MAX_PARENT_PIDS, process) == 0)
    // {
    //     return FLT_PREOP_SUCCESS_NO_CALLBACK;
    // }
    KdPrint(("WRITING FULL PATH: %wZ\r\n", fullPath));
    if (wcsncmp(fullPath->Buffer, rootDir, wcslen(rootDir)) == 0 && Data->Iopb->Parameters.Write.WriteBuffer != NULL)
    {
        // PUNICODE_STRING copyPath = getCopyPAth(Data, fullPath);

        // copyFile(FltObjects->Filter, FltObjects->Instance, Data->Iopb->TargetFileObject, copyPath);
        NTSTATUS status;
        OBJECT_ATTRIBUTES objAttr;
        IO_STATUS_BLOCK ioStatusBlock;
        HANDLE fileHandle;
        PFILE_OBJECT fileObject;
        UNICODE_STRING copyPath;
        RtlInitUnicodeString(&copyPath, L"\\Device\\HarddiskVolume2\\Users\\WDAGUtilityAccount\\Desktop\\folder\\some.txt");
        InitializeObjectAttributes(&objAttr, &copyPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
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
                                 FILE_OPEN_IF,                                           /* CreateDisposition */
                                 FILE_SYNCHRONOUS_IO_NONALERT,                           /* CreateOptions     */
                                 NULL,                                                   /* EaBuffer          */
                                 0,                                                      /* EaLength          */
                                 IO_FORCE_ACCESS_CHECK);

        KdPrint(("copyPath PATH: %wZ\r\n", copyPath));
        KdPrint(("FULL PATH: %wZ\r\n", fullPath));
        Data->Iopb->TargetFileObject = fileObject;
        FltSetCallbackDataDirty(Data);
        KdPrint(("NEW CONTENT: %s\r\n", Data->Iopb->Parameters.Write.WriteBuffer));
        PPRE_2_POST_CONTEXT p2pCtx = (PPRE_2_POST_CONTEXT)ExAllocatePool2(NonPagedPoolNx, sizeof(PRE_2_POST_CONTEXT), 'p2pC');
        if (p2pCtx == NULL)
        {
            FltClose(fileHandle);
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        p2pCtx->fileHandle = &fileHandle;
        *CompletionContext = p2pCtx;
    }
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
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

    PUNICODE_STRING fullPath = getFullPath(Data);
    PUNICODE_STRING copyPath = getCopyPAth(Data, fullPath);

    OBJECT_ATTRIBUTES objAttr;
    IO_STATUS_BLOCK ioStatusBlock;
    HANDLE fileHandle;
    PFILE_OBJECT fileObject;

    if (wcsncmp(fullPath->Buffer, rootDir, wcslen(rootDir)) == 0 && Data->Iopb->Parameters.Write.WriteBuffer != NULL)
    {
        NTSTATUS status;
        UNICODE_STRING URootDir;
        PUNICODE_STRING fullPath2 = (PUNICODE_STRING)ExAllocatePool(NonPagedPoolNx, sizeof(UNICODE_STRING));
        fullPath2->MaximumLength = (USHORT)(fullPath->MaximumLength + sizeof(WCHAR) * 2);
        fullPath2->Buffer = (WCHAR *)ExAllocatePool2(POOL_FLAG_PAGED, fullPath->MaximumLength, 'MGT1');
        fullPath2->Length = 0;
        RtlInitUnicodeString(&URootDir, rootDir);
        KdPrint(("Renaming to1 %wZ\r\n", fullPath));

        RtlCopyUnicodeString(fullPath2, fullPath);
        UNICODE_STRING newPath = ReplaceDeviceName(fullPath2, getDeviceName(Data->Iopb->TargetFileObject->DeviceObject), &URootDir);
        PFILE_RENAME_INFORMATION renameInfo = (PFILE_RENAME_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(FILE_RENAME_INFORMATION) + newPath.MaximumLength + sizeof(WCHAR), 'MGT1');
        CreateDirectory(FilterHandle, FltObjects->Instance, newPath.Buffer, TRUE);
        if (renameInfo == NULL || &newPath == NULL)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        renameInfo->FileNameLength = newPath.Length;
        renameInfo->ReplaceIfExists = TRUE;
        RtlCopyMemory(renameInfo->FileName, newPath.Buffer, newPath.Length);

        InitializeObjectAttributes(&objAttr, copyPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
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
                                 FILE_OPEN,                                              /* CreateDisposition */
                                 FILE_SYNCHRONOUS_IO_NONALERT,                           /* CreateOptions     */
                                 NULL,                                                   /* EaBuffer          */
                                 0,                                                      /* EaLength          */
                                 IO_FORCE_ACCESS_CHECK);

        if (!NT_SUCCESS(status))
        {
            ExFreePool(renameInfo);
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        status = FltSetInformationFile(FltObjects->Instance, FltObjects->FileObject, renameInfo, renameInfo->FileNameLength, FileRenameInformation);

        PFILE_RENAME_INFORMATION renameInfo2 = (PFILE_RENAME_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(FILE_RENAME_INFORMATION) + fullPath->MaximumLength + sizeof(WCHAR), 'MGT1');
        if (renameInfo2 == NULL)
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }
        renameInfo2->FileNameLength = fullPath->Length;
        renameInfo2->ReplaceIfExists = TRUE;
        RtlCopyMemory(renameInfo2->FileName, fullPath->Buffer, fullPath->Length);
        status = FltSetInformationFile(FltObjects->Instance, fileObject, renameInfo2, renameInfo2->FileNameLength, FileRenameInformation);
        KdPrint(("Renaming to %wZ\r\n", fullPath));
        if (NT_SUCCESS(status))
        {
            KdPrint(("RENAMED SUCCESSFULY\r\n"));
        }
        else
        {
            KdPrint(("Failed to rename Status: 0x%X\n", status));
        }
        // ExFreePool(renameInfo);
        // ExFreePool(renameInfo2);
        FltClose(fileHandle);
    }
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
    KdPrint(("FULL PATH: %wZ\r\n", fullPath));
    char access[250];
    access[0] = '\0';
    char options[250];
    options[0] = '\0';
    getDesiredAccessRights(Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess, access);
    getDesiredAccessRights((Data->Iopb->Parameters.Create.Options >> 24) && 0xFF, options);
    KdPrint(("desired access %s\r\n", access));
    KdPrint(("OPEN OPTIONS %s\r\n", options));
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
    KdPrint(("Full PATH: %wZ\r\n", &fullPath));

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
    KdPrint(("RENAMING FULL PATH: %wZ\r\n", fullPath));
    switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass)
    {
    case FileEndOfFileInformation:
        PFILE_END_OF_FILE_INFORMATION endInfo = (PFILE_END_OF_FILE_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
        KdPrint(("END OF FILE: %lu\r\n", endInfo->EndOfFile));
    }
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
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
