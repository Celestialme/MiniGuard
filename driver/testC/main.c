#include <stdio.h>
#include <ntdef.h>
#include <windows.h>
#include <ntstatus.h>

PUNICODE_STRING getUnicode()
{
    PUNICODE_STRING ret = (PUNICODE_STRING)malloc(sizeof(UNICODE_STRING));
    RtlInitUnicodeString(ret, L"\\Device\\HarddiskVolume1");
    return ret;
}

void main()
{

    UNICODE_STRING fileName;
    UNICODE_STRING fullPath;

    UNICODE_STRING deviceName = *getUnicode();
    PUNICODE_STRING deviceName2 = getUnicode();
    printf("deviceName is %S\n", deviceName2->Buffer);
    RtlInitUnicodeString(&fileName, L"\\Users\\WDAGUtilityAccount\\Desktop\\bew\\some33.txt");

    fullPath.Buffer = (WCHAR *)malloc(deviceName2->MaximumLength + fileName.MaximumLength + sizeof(WCHAR) * 2);
    fullPath.Length = 0;
    fullPath.MaximumLength = (USHORT)(deviceName2->MaximumLength + fileName.MaximumLength + sizeof(WCHAR) * 2);

    NTSTATUS status = RtlAppendUnicodeStringToString(&fullPath, deviceName2);
    if (status == STATUS_BUFFER_TOO_SMALL)
    {
        printf("STATUS_BUFFER_TOO_SMALL\n");
    }
    status = RtlAppendUnicodeStringToString(&fullPath, &fileName);
    if (status == STATUS_BUFFER_TOO_SMALL)
    {
        printf("STATUS_BUFFER_TOO_SMALL\n");
    }
    printf("fileName is %S\n", fullPath.Buffer);
}