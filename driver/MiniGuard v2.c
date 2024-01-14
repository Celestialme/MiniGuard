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
typedef struct _CTX_STREAMHANDLE_CONTEXT
{

    ULONG createDisposition;

} CTX_STREAMHANDLE_CONTEXT, *PCTX_STREAMHANDLE_CONTEXT;
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
MiniGuardPreCreateOperation(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID *CompletionContext)
{

    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    PUNICODE_STRING fullPath = getFullPath(Data);
    if (fullPath == NULL)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }
    PReply reply_struct;
    // ULONG process = FltGetRequestorProcessId(Data);
    if (
        // isInArray(pids, MAX_PARENT_PIDS, process) == 0 ||
        !FLT_IS_IRP_OPERATION(Data) || wcsncmp(fullPath->Buffer, rootDir, wcslen(rootDir)) != 0)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    if (Data->Iopb->Parameters.Create.Options & FILE_DELETE_ON_CLOSE)
    {
        reply_struct = sendMessage(FltObjects->Filter, ClientPort, 4, fullPath);

        if (reply_struct != NULL)
        {

            ExFreePool(reply_struct);
        }
    }

    LONG createDisposition = (Data->Iopb->Parameters.Create.Options >> 24) & 0xFF;

    if (createDisposition & FILE_CREATE)
    {
        if (!NT_SUCCESS(FileExists(FilterHandle, FltObjects->Instance, fullPath->Buffer)))
        {
            reply_struct = sendMessage(FltObjects->Filter, ClientPort, 1, fullPath);
            KdPrint(("CREATE FILE: %wZ\r\n", fullPath));
            if (reply_struct != NULL)
            {
                ExFreePool(reply_struct);
            }
        }
    }

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
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
    if (fullPath == NULL)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }
    // ULONG process = FltGetRequestorProcessId(Data);
    if (
        // isInArray(pids, MAX_PARENT_PIDS, process) == 0 ||
        !FLT_IS_IRP_OPERATION(Data) || wcsncmp(fullPath->Buffer, rootDir, wcslen(rootDir)) != 0)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    KdPrint(("WRITE FILE: %wZ\r\n", fullPath));

    PReply reply_struct = sendMessage(FltObjects->Filter, ClientPort, 2, fullPath);

    if (reply_struct != NULL)
    {

        ExFreePool(reply_struct);
    }

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}
FLT_PREOP_CALLBACK_STATUS
MiniGuardPreSetInfoOperation(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID *CompletionContext)
{

    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    PUNICODE_STRING fullPath = getFullPath(Data);
    if (fullPath == NULL)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }
    PReply reply_struct;
    // ULONG process = FltGetRequestorProcessId(Data);
    if (
        // isInArray(pids, MAX_PARENT_PIDS, process) == 0 ||
        !FLT_IS_IRP_OPERATION(Data) || wcsncmp(fullPath->Buffer, rootDir, wcslen(rootDir)) != 0)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass)
    {
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
        PUNICODE_STRING combined = (PUNICODE_STRING)ExAllocatePool(NonPagedPoolNx, sizeof(UNICODE_STRING));
        combined->MaximumLength = (USHORT)(fullPath->MaximumLength + DestinationFileNameInformation->Name.MaximumLength + sizeof(WCHAR) * 3);
        combined->Buffer = (WCHAR *)ExAllocatePool2(POOL_FLAG_PAGED, combined->MaximumLength, 'MGT1');
        combined->Length = 0;

        RtlAppendUnicodeStringToString(combined, fullPath);
        RtlAppendUnicodeToString(combined, L"->");
        RtlAppendUnicodeStringToString(combined, &DestinationFileNameInformation->Name);

        reply_struct = sendMessage(FltObjects->Filter, ClientPort, 3, combined);

        if (reply_struct != NULL)
        {
            ExFreePool(reply_struct);
        }

        return FLT_PREOP_SUCCESS_NO_CALLBACK;

    case FileDispositionInformation:
    case FileDispositionInformationEx:
        if (((FILE_DISPOSITION_INFORMATION *)
                 Data->Iopb->Parameters.SetFileInformation.InfoBuffer)
                ->DeleteFile)
        {

            reply_struct = sendMessage(FltObjects->Filter, ClientPort, 4, fullPath);

            if (reply_struct != NULL)
            {
                ExFreePool(reply_struct);
            }
        }
        return FLT_PREOP_SUCCESS_NO_CALLBACK;

    case FileEndOfFileInformation:
        PFILE_END_OF_FILE_INFORMATION endInfo = (PFILE_END_OF_FILE_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
        if (endInfo->EndOfFile.QuadPart == 0)
        {

            reply_struct = sendMessage(FltObjects->Filter, ClientPort, 2, fullPath);

            if (reply_struct != NULL)
            {
                ExFreePool(reply_struct);
            }
        }
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    KdPrint(("Unhandled FileInformationClass: %d\r\n", Data->Iopb->Parameters.SetFileInformation.FileInformationClass));
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    {IRP_MJ_CREATE,
     0,
     MiniGuardPreCreateOperation,
     NULL},
    {IRP_MJ_SET_INFORMATION,
     0,
     MiniGuardPreSetInfoOperation,
     NULL},
    {IRP_MJ_WRITE,
     0,
     MiniGuardPreWriteOperation,
     NULL},
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
