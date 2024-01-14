#include "utils.h"
#include <fltKernel.h>
int findFirstsZero(LONG array[], int arraySize)
{
    int index = -1; // Initialize index to -1 (indicating not found)
    for (int i = 0; i < arraySize; i++)
    {
        if (array[i] == 0)
        {
            index = i;
            break; // Break out of the loop once the first zero is found
        }
    }
    return index;
}

// Function to check if a number is in the array
int isInArray(LONG array[], int size, int target)
{
    for (int i = 0; i < size; i++)
    {
        if (array[i] == target)
        {
            return 1; // Return 1 if the number is found in the array
        }
    }
    return 0; // Return 0 if the number is not found in the array
}

void getDesiredAccessRights(int accessCode, char access[500])
{

    // Check each access right and push into the array
    if (accessCode & FILE_READ_DATA)
    {
        strcat(access, "FILE_READ_DATA | ");
    }
    if (accessCode & FILE_LIST_DIRECTORY)
    {
        strcat(access, "FILE_LIST_DIRECTORY | ");
    }
    if (accessCode & FILE_WRITE_DATA)
    {
        strcat(access, "FILE_WRITE_DATA | ");
    }
    if (accessCode & FILE_ADD_FILE)
    {
        strcat(access, "FILE_ADD_FILE | ");
    }
    if (accessCode & FILE_APPEND_DATA)
    {
        strcat(access, "FILE_APPEND_DATA | ");
    }
    if (accessCode & FILE_ADD_SUBDIRECTORY)
    {
        strcat(access, "FILE_ADD_SUBDIRECTORY | ");
    }
    if (accessCode & FILE_CREATE_PIPE_INSTANCE)
    {
        strcat(access, "FILE_CREATE_PIPE_INSTANCE | ");
    }

    if (accessCode & FILE_READ_EA)
    {
        strcat(access, "FILE_READ_EA | ");
    }
    if (accessCode & FILE_WRITE_EA)
    {
        strcat(access, "FILE_WRITE_EA | ");
    }
    if (accessCode & FILE_GENERIC_READ)
    {
        strcat(access, "FILE_GENERIC_READ | ");
    }
    if (accessCode & FILE_GENERIC_WRITE)
    {
        strcat(access, "FILE_GENERIC_WRITE | ");
    }

    strcat(access, "\n");
    // Mark the end of the array with a NULL pointer
};

void getCreateDisposition(int accessCode, char access[500])
{

    if (accessCode & FILE_SUPERSEDE)
    {
        strcat(access, "FILE_SUPERSEDE | ");
    }
    if (accessCode & FILE_CREATE)
    {
        strcat(access, "FILE_CREATE | ");
    }
    if (accessCode & FILE_OPEN)
    {
        strcat(access, "FILE_OPEN | ");
    }
    if (accessCode & FILE_OPEN_IF)
    {
        strcat(access, "FILE_OPEN_IF | ");
    }
    if (accessCode & FILE_OVERWRITE)
    {
        strcat(access, "FILE_OVERWRITE | ");
    }
    if (accessCode & FILE_OVERWRITE_IF)
    {
        strcat(access, "FILE_OVERWRITE_IF | ");
    }
}

NTSTATUS toDosName(PFILE_OBJECT FileObject, PUNICODE_STRING destination)
{

    POBJECT_NAME_INFORMATION dosName;
    NTSTATUS status = IoQueryFileDosDeviceName(FileObject, &dosName);
    if (NT_SUCCESS(status))
    {
        RtlInitUnicodeString(destination, dosName->Name.Buffer);

        ExFreePool(dosName);
    }
    return status;
}

NTSTATUS FileExists(PFLT_FILTER Filter, PFLT_INSTANCE Instance, const WCHAR *filePath)
{
    NTSTATUS status;
    UNICODE_STRING fileName;
    OBJECT_ATTRIBUTES objAttr;
    IO_STATUS_BLOCK ioStatusBlock;
    HANDLE fileHandle;

    // Initialize the file name
    RtlInitUnicodeString(&fileName, filePath);

    // Initialize the object attributes
    InitializeObjectAttributes(&objAttr, &fileName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    // Try to open the file using FltCreateFile
    status = FltCreateFile(
        Filter,                             // Filter (NULL for this example)
        Instance,                           // Instance (NULL for this example)
        &fileHandle,                        // File handle
        SYNCHRONIZE,                        // Desired access for checking existence
        &objAttr,                           // Object attributes
        &ioStatusBlock,                     // I/O status block
        NULL,                               // Allocation size (optional)
        FILE_ATTRIBUTE_NORMAL,              // File attributes
        FILE_SHARE_READ | FILE_SHARE_WRITE, // Share access
        FILE_OPEN,                          // Create disposition
        FILE_SYNCHRONOUS_IO_NONALERT,       // Create options
        NULL,                               // Ea buffer (optional)
        0,                                  // Ea length (optional)
        0);

    if (NT_SUCCESS(status))
    {
        // File exists
        FltClose(fileHandle);
        return STATUS_SUCCESS;
    }
    else if (status == STATUS_OBJECT_NAME_NOT_FOUND)
    {
        // File does not exist
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }
    else
    {
        // Handle other errors
        return status;
    }
}

NTSTATUS CreateDirectory(PFLT_FILTER Filter, PFLT_INSTANCE Instance, const WCHAR *directoryName, BOOLEAN onlyDeps)
{

    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING unicodeDirectoryName;
    HANDLE directoryHandle;
    IO_STATUS_BLOCK ioStatusBlock;
    size_t length = wcslen(directoryName) + 1;
    WCHAR *currentPath = (WCHAR *)ExAllocatePoolWithTag(PagedPool, length * sizeof(WCHAR), 'MGT');
    wcsncpy(currentPath, directoryName, length);

    if (NT_SUCCESS(FileExists(Filter, Instance, currentPath)))
    {
        KdPrint(("Directory already exists: %s\r\n", currentPath));

        return STATUS_SUCCESS;
    }
    else
    {
        KdPrint(("Creating directory: %S\r\n", currentPath));
    }

    WCHAR *lastBackslash = wcsrchr(currentPath, L'\\');
    if (lastBackslash != NULL)
    {
        *lastBackslash = L'\0';
        CreateDirectory(Filter, Instance, currentPath, FALSE);
    }

    RtlInitUnicodeString(&unicodeDirectoryName, directoryName);
    InitializeObjectAttributes(
        &objectAttributes,
        &unicodeDirectoryName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL);

    if (!onlyDeps)
    {

        NTSTATUS status = FltCreateFile(
            Filter,
            Instance,
            &directoryHandle,
            FILE_GENERIC_WRITE | SYNCHRONIZE,
            &objectAttributes,
            &ioStatusBlock,
            NULL,
            FILE_ATTRIBUTE_NORMAL,
            0,
            FILE_OPEN_IF,
            FILE_DIRECTORY_FILE,
            NULL,
            0, 0);

        if (!NT_SUCCESS(status))
        {
            KdPrint(("Failed to create directory %s. Status: 0x%X\r\n", directoryName, status));
            return status;
        }
        FltClose(directoryHandle);
    }

    return STATUS_SUCCESS;
}

PUNICODE_STRING getDeviceName(PDEVICE_OBJECT DeviceObject)
{
    PWCHAR tempDeviceName = NULL;
    PUNICODE_STRING deviceName = (PUNICODE_STRING)ExAllocatePool(NonPagedPoolNx, sizeof(UNICODE_STRING));
    ULONG nameLength = 0;
    NTSTATUS status = IoGetDeviceProperty(DeviceObject,
                                          DevicePropertyPhysicalDeviceObjectName, 0,
                                          NULL, &nameLength);
    if ((nameLength != 0) && (status == STATUS_BUFFER_TOO_SMALL))
    {
        tempDeviceName = ExAllocatePoolZero(NonPagedPoolNx, nameLength, 'MGT5');
        status = IoGetDeviceProperty(DeviceObject,
                                     DevicePropertyPhysicalDeviceObjectName,
                                     nameLength, tempDeviceName, &nameLength);
        if (NT_SUCCESS(status))
        {
            RtlInitUnicodeString(deviceName, tempDeviceName);
        }
        ExFreePool(tempDeviceName);
    }
    else
    {
        RtlInitUnicodeString(deviceName, L"");
    }
    return deviceName;
}

UNICODE_STRING ReplaceDeviceName(UNICODE_STRING *fileName, UNICODE_STRING *deviceName, UNICODE_STRING *rootDir)
{
    UNICODE_STRING MiniGuard;
    RtlInitUnicodeString(&MiniGuard, L"MiniGuard");
    UNICODE_STRING result;
    USHORT remainingLength = fileName->Length - deviceName->Length;
    fileName->Length = remainingLength;
    fileName->MaximumLength = remainingLength;
    fileName->Buffer += deviceName->Length / sizeof(WCHAR);

    result.MaximumLength = (USHORT)(rootDir->MaximumLength + fileName->MaximumLength + MiniGuard.MaximumLength + sizeof(WCHAR) * 2);
    result.Buffer = (WCHAR *)ExAllocatePool2(POOL_FLAG_PAGED, result.MaximumLength, 'MGT2');
    result.Length = 0;
    RtlAppendUnicodeStringToString(&result, rootDir);
    RtlAppendUnicodeStringToString(&result, &MiniGuard);
    RtlAppendUnicodeStringToString(&result, fileName);
    return result;
}

UNICODE_STRING getMiniGuardPath(UNICODE_STRING *rootDir)
{
    UNICODE_STRING MiniGuard;
    RtlInitUnicodeString(&MiniGuard, L"MiniGuard");
    UNICODE_STRING result;

    result.MaximumLength = (USHORT)(rootDir->MaximumLength + MiniGuard.MaximumLength + sizeof(WCHAR) * 2);
    result.Buffer = (WCHAR *)ExAllocatePool2(POOL_FLAG_PAGED, result.MaximumLength, 'MGT2');
    result.Length = 0;
    RtlAppendUnicodeStringToString(&result, rootDir);
    RtlAppendUnicodeStringToString(&result, &MiniGuard);
    return result;
}

void copyFile(PFLT_FILTER Filter, PFLT_INSTANCE Instance, PFILE_OBJECT FileObject, PUNICODE_STRING copyPath)
{

    OBJECT_ATTRIBUTES objAttr;
    IO_STATUS_BLOCK ioStatusBlock;
    HANDLE fileHandle;
    PFILE_OBJECT fileObject;
    FILE_STANDARD_INFORMATION fileInfo;
    NTSTATUS status;
    status = FltQueryInformationFile(
        Instance,
        FileObject,
        &fileInfo,
        sizeof(fileInfo),
        FileStandardInformation,
        NULL);
    if (!NT_SUCCESS(status))
    {
        return;
    }
    LONGLONG fileSize = fileInfo.EndOfFile.QuadPart;
    PVOID buffer = ExAllocatePoolZero(NonPagedPool, fileSize, 'MTG1');
    if (buffer == NULL)
    {
        return;
    }
    LARGE_INTEGER FileOffset;
    FileOffset.QuadPart = 0;
    status = FltReadFile(
        Instance,
        FileObject,
        &FileOffset,
        (LONG)fileSize,
        buffer,
        0,
        NULL,
        NULL,
        NULL);
    if (!NT_SUCCESS(status))
    {
        ExFreePool(buffer);
        return;
    }
    InitializeObjectAttributes(&objAttr, copyPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    status = FltCreateFileEx(Filter,                       /* Filter            */
                             Instance,                     /* Instance          */
                             &fileHandle,                  /* FileHandle        */
                             &fileObject,                  /* FileObject        */
                             GENERIC_WRITE,                /* DesiredAccess     */
                             &objAttr,                     /* ObjectAttributes  */
                             &ioStatusBlock,               /* IoStatusBlock     */
                             NULL,                         /* AllocationSize    */
                             FILE_ATTRIBUTE_NORMAL,        /* FileAttributes    */
                             0,                            /* ShareAccess       */
                             FILE_OVERWRITE_IF,            /* CreateDisposition */
                             FILE_SYNCHRONOUS_IO_NONALERT, /* CreateOptions     */
                             NULL,                         /* EaBuffer          */
                             0,                            /* EaLength          */
                             IO_FORCE_ACCESS_CHECK);
    if (!NT_SUCCESS(status))
    {
        ExFreePool(buffer);
        return;
    }

    FltWriteFile(
        Instance,
        fileObject,
        &FileOffset,
        (LONG)fileSize,
        buffer,
        0,
        NULL,
        NULL,
        NULL);
    KdPrint(("SIZE: %lu\r\n", fileSize));
    KdPrint(("OLD CONTENT: %s\r\n", buffer));
    ExFreePool(buffer);
    FltClose(fileHandle);
}

// PUNICODE_STRING getFullPath(PFLT_CALLBACK_DATA Data)
// {
//     PUNICODE_STRING deviceName = getDeviceName(Data->Iopb->TargetFileObject->DeviceObject);
//     UNICODE_STRING fileName = Data->Iopb->TargetFileObject->FileName;
//     PUNICODE_STRING fullPath = (PUNICODE_STRING)ExAllocatePool(NonPagedPoolNx, sizeof(UNICODE_STRING));
//     fullPath->MaximumLength = (USHORT)(deviceName->MaximumLength + fileName.MaximumLength + sizeof(WCHAR) * 2);
//     fullPath->Buffer = (WCHAR *)ExAllocatePool2(POOL_FLAG_PAGED, fullPath->MaximumLength, 'MGT1');
//     fullPath->Length = 0;

//     RtlAppendUnicodeStringToString(fullPath, deviceName);
//     RtlAppendUnicodeStringToString(fullPath, &fileName);
//     return fullPath;
// }
PUNICODE_STRING getFullPath(PFLT_CALLBACK_DATA Data)
{
    NTSTATUS status;
    PFLT_FILE_NAME_INFORMATION fileNameInfo;

    // Get the file name information
    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &fileNameInfo);

    if (!NT_SUCCESS(status))
    {
        return NULL;
    }
    PUNICODE_STRING fullPath = (PUNICODE_STRING)ExAllocatePool(NonPagedPoolNx, sizeof(UNICODE_STRING));
    fullPath->MaximumLength = (USHORT)(fileNameInfo->Name.MaximumLength);
    fullPath->Buffer = (WCHAR *)ExAllocatePool2(POOL_FLAG_PAGED, fullPath->MaximumLength, 'MGT1');
    fullPath->Length = 0;
    RtlAppendUnicodeStringToString(fullPath, &fileNameInfo->Name);
    FltReleaseFileNameInformation(fileNameInfo);
    return fullPath;
}
PUNICODE_STRING getCopyPAth(PFLT_CALLBACK_DATA Data, PUNICODE_STRING fullPath)
{
    PUNICODE_STRING copyPath = (PUNICODE_STRING)ExAllocatePool(NonPagedPoolNx, sizeof(UNICODE_STRING));
    copyPath->MaximumLength = (USHORT)(fullPath->MaximumLength + Data->Iopb->TargetFileObject->FileName.MaximumLength + sizeof(WCHAR) * 10);
    copyPath->Buffer = (WCHAR *)ExAllocatePool2(POOL_FLAG_PAGED, copyPath->MaximumLength, 'MGT1');
    copyPath->Length = 0;

    RtlAppendUnicodeStringToString(copyPath, fullPath);
    RtlAppendUnicodeToString(copyPath, L".copy");
    return copyPath;
}

void writeToFile(PFLT_INSTANCE Instance, PFILE_OBJECT fileObject, PUNICODE_STRING text)
{
    UNREFERENCED_PARAMETER(Instance);
    UNREFERENCED_PARAMETER(fileObject);

    UNICODE_STRING newLine;
    RtlInitUnicodeString(&newLine, L"\r\n");
    LARGE_INTEGER FileOffset;
    FileOffset.QuadPart = -1;
    PUNICODE_STRING terminated = (PUNICODE_STRING)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(UNICODE_STRING), 'MGT1');
    terminated->MaximumLength = (USHORT)(text->MaximumLength + newLine.MaximumLength);
    terminated->Buffer = (WCHAR *)ExAllocatePool2(POOL_FLAG_PAGED, terminated->MaximumLength, 'MGT1');
    terminated->Length = 0;

    RtlAppendUnicodeStringToString(terminated, text);
    RtlAppendUnicodeStringToString(terminated, &newLine);

    FltWriteFile(
        Instance,
        fileObject,
        &FileOffset,
        terminated->MaximumLength,
        terminated->Buffer,
        0,
        NULL,
        NULL,
        NULL);
}

PFILE_HANDLE_OBJECT getLogFile(PFLT_FILTER Filter, PUNICODE_STRING fullPath)
{
    OBJECT_ATTRIBUTES objAttr;
    IO_STATUS_BLOCK ioStatusBlock;
    HANDLE fileHandle;
    PFILE_OBJECT fileObject;
    NTSTATUS status;

    InitializeObjectAttributes(&objAttr, fullPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    status = FltCreateFileEx(Filter,                             /* Filter            */
                             NULL,                               /* Instance          */
                             &fileHandle,                        /* FileHandle        */
                             &fileObject,                        /* FileObject        */
                             GENERIC_WRITE | GENERIC_READ,       /* DesiredAccess     */
                             &objAttr,                           /* ObjectAttributes  */
                             &ioStatusBlock,                     /* IoStatusBlock     */
                             NULL,                               /* AllocationSize    */
                             FILE_ATTRIBUTE_NORMAL,              /* FileAttributes    */
                             FILE_SHARE_READ | FILE_SHARE_WRITE, /* ShareAccess       */
                             FILE_OPEN_IF,                       /* CreateDisposition */
                             FILE_SYNCHRONOUS_IO_NONALERT,       /* CreateOptions     */
                             NULL,                               /* EaBuffer          */
                             0,                                  /* EaLength          */
                             IO_FORCE_ACCESS_CHECK);

    if (!NT_SUCCESS(status))
    {
        KdPrint(("FILE NOT CREATED: %wZ, status: %x\r\n", fullPath, status));
        return NULL;
    }
    else
    {
        PFILE_HANDLE_OBJECT result = ExAllocatePool2(POOL_FLAG_PAGED, sizeof(FILE_HANDLE_OBJECT), 'MGT1');
        if (!result)
        {
            return NULL;
        }

        result->fileHandle = fileHandle;
        result->fileObject = fileObject;

        return result;
    }
}

PReply sendMessage(PFLT_FILTER Filter, PFLT_PORT ClientPort, int operation, PUNICODE_STRING data)
{
    ULONG reply_buffer_size = 1024;
    PVOID reply_buffer = ExAllocatePool2(POOL_FLAG_PAGED, reply_buffer_size, 'MGT2');
    if (reply_buffer == NULL || ClientPort == NULL)
    {
        return NULL;
    }
    SEND_MESSAGE message;
    message.operation_type = operation;
    RtlCopyMemory(&message.data, data->Buffer, data->MaximumLength);
    LARGE_INTEGER timeout;
    timeout.QuadPart = -10 * 1000 * 1000 * 10;
    FltSendMessage(Filter, &ClientPort, &message, sizeof(SEND_MESSAGE), reply_buffer, &reply_buffer_size, &timeout);

    return (PReply)reply_buffer;
}

BOOLEAN isIgnored(PUNICODE_STRING fullPath, PMESSAGE FltMessage)
{
    if (FltMessage == NULL)
    {
        return FALSE;
    }

    // KdPrint(("INSIDE array0 %ws \r\n", FltMessage->array[0]));
    for (int i = 0; i < MAX_IGNORE_DIRS; i++)
    {
        if (FltMessage->array[i] == NULL || FltMessage->array[i][0] == 0)
        {
            return FALSE;
        }
        if (wcsncmp(fullPath->Buffer, FltMessage->array[i], wcslen(FltMessage->array[i])) == 0)
        {
            KdPrint(("INSIDE STARTS WITH %ws\r\n", FltMessage->array[i]));
            return TRUE;
        }
    }
    return FALSE;
}
NTSTATUS TerminateProcess(ULONG ProcessId)
{
    HANDLE hProcess;
    OBJECT_ATTRIBUTES objAttr;
    CLIENT_ID clientId;
    NTSTATUS status;

    // Initialize the OBJECT_ATTRIBUTES structure and CLIENT_ID
    InitializeObjectAttributes(&objAttr, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);
    clientId.UniqueProcess = (HANDLE)ProcessId;
    clientId.UniqueThread = NULL;

    // Open the target process
    status = ZwOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &objAttr, &clientId);
    if (!NT_SUCCESS(status))
    {
        KdPrint(("Failed to open process (PID %lu) with status 0x%X\n", ProcessId, status));
        return status;
    }

    // Terminate the process
    status = ZwTerminateProcess(hProcess, 0);
    if (!NT_SUCCESS(status))
    {
        KdPrint(("Failed to terminate process (PID %lu) with status 0x%X\n", ProcessId, status));
    }

    // Close the process handle
    ZwClose(hProcess);

    return status;
}