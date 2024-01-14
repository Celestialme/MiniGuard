#include <fltKernel.h>
#include <ntddk.h>
#define MAX_IGNORE_DIRS 10

typedef struct _FILE_HANDLE_OBJECT
{
    PHANDLE fileHandle;
    PFILE_OBJECT fileObject;
} FILE_HANDLE_OBJECT, *PFILE_HANDLE_OBJECT;
typedef struct _Reply
{
    WCHAR data[40];

} Reply, *PReply;

typedef struct _SEND_MESSAGE
{
    int operation_type;
    WCHAR data[2048];

} SEND_MESSAGE, *PSEND_MESSAGE;
typedef struct _MESSAGE
{
    LONG pid;
    WCHAR array[MAX_IGNORE_DIRS][2048];

} MESSAGE, *PMESSAGE;
int findFirstsZero(LONG array[], int arraySize);
int isInArray(LONG array[], int size, int target);
void getDesiredAccessRights(int accessCode, char access[250]);
void getCreateDisposition(int accessCode, char access[500]);
NTSTATUS FileExists(PFLT_FILTER Filter, PFLT_INSTANCE Instance, const WCHAR *filePath);
NTSTATUS CreateDirectory(PFLT_FILTER Filter, PFLT_INSTANCE Instance, const WCHAR *directoryName, BOOLEAN onlyDeps);
PUNICODE_STRING getDeviceName(PDEVICE_OBJECT DeviceObject);
UNICODE_STRING ReplaceDeviceName(UNICODE_STRING *fileName, UNICODE_STRING *deviceName, UNICODE_STRING *rootDir);
UNICODE_STRING getMiniGuardPath(UNICODE_STRING *rootDir);
void copyFile(PFLT_FILTER Filter, PFLT_INSTANCE Instance, PFILE_OBJECT FileObject, PUNICODE_STRING copyPath);
PUNICODE_STRING getFullPath(PFLT_CALLBACK_DATA Data);
PUNICODE_STRING getCopyPAth(PFLT_CALLBACK_DATA Data, PUNICODE_STRING fullPath);
void writeToFile(PFLT_INSTANCE Instance, PFILE_OBJECT fileObject, PUNICODE_STRING text);
PFILE_HANDLE_OBJECT getLogFile(PFLT_FILTER Filter, PUNICODE_STRING fullPath);
PReply sendMessage(PFLT_FILTER Filter, PFLT_PORT ClientPort, int operation, PUNICODE_STRING data);
BOOLEAN isIgnored(PUNICODE_STRING fullPath, PMESSAGE FltMessage);
NTSTATUS TerminateProcess(ULONG ProcessId);