#include <stdio.h>
#include <ntdef.h>
#include <windows.h>
#include <ntstatus.h>
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
    result.Buffer = (WCHAR *)malloc(result.MaximumLength);
    result.Length = 0;
    RtlAppendUnicodeStringToString(&result, rootDir);
    RtlAppendUnicodeStringToString(&result, &MiniGuard);
    RtlAppendUnicodeStringToString(&result, fileName);
    return result;
}
void main()
{
    UNICODE_STRING fileName;
    UNICODE_STRING deviceName;
    UNICODE_STRING rootDir;
    RtlInitUnicodeString(&rootDir, L"\\Device\\HarddiskVolume2\\Users\\WDAGUtilityAccount\\Desktop\\folder\\");
    RtlInitUnicodeString(&deviceName, L"\\Device\\HarddiskVolume1");
    RtlInitUnicodeString(&fileName, L"\\Device\\HarddiskVolume1\\Users\\WDAGUtilityAccount\\Desktop\\some33.txt");

    printf("fileName is %S\n", ReplaceDeviceName(&fileName, &deviceName, &rootDir).Buffer);
}