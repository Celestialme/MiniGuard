PFLT_FILE_NAME_INFORMATION FileNameInformation = NULL;

status = FltGetFileNameInformation(Data, FLT_FILE_NAME_QUERY_FILESYSTEM_ONLY | FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &FileNameInformation);
if (NT_SUCCESS(status))
{
    status = FltParseFileNameInformation(FileNameInformation);

    if (NT_SUCCESS(status))
    {

        if (wcsstr(FileNameInformation->Name.Buffer, L"some2.txt") != NULL)
        {
            switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass)
            {
            case FileRenameInformation:
            case FileRenameInformationEx:
                UNICODE_STRING newFilePath;
                RtlInitUnicodeString(&newFilePath, L"\\Device\\HarddiskVolume2\\Users\\WDAGUtilityAccount\\Desktop\\bew\\some33.txt");
                PFILE_RENAME_INFORMATION renameInfo = (PFILE_RENAME_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
                PFILE_RENAME_INFORMATION newRenameInfo = (PFILE_RENAME_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(FILE_RENAME_INFORMATION) + newFilePath.Length + sizeof(WCHAR), 'MGT1');
                PFLT_FILE_NAME_INFORMATION DestinationFileNameInformation = NULL;
                FltGetDestinationFileNameInformation(
                    FltObjects->Instance,
                    FltObjects->FileObject,
                    renameInfo->RootDirectory,
                    renameInfo->FileName,
                    renameInfo->FileNameLength,
                    FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
                    &DestinationFileNameInformation);

                newRenameInfo->RootDirectory = renameInfo->RootDirectory;
                newRenameInfo->FileNameLength = newFilePath.Length;
                newRenameInfo->ReplaceIfExists = TRUE;
                CreateDirectory(FilterHandle, FltObjects->Instance, newFilePath.Buffer, TRUE);
                RtlCopyMemory(newRenameInfo->FileName, newFilePath.Buffer, newFilePath.Length);
                FltSetInformationFile(FltObjects->Instance, FltObjects->FileObject, newRenameInfo, newRenameInfo->FileNameLength, FileRenameInformation);
                ExFreePool(newRenameInfo);
                FltSetCallbackDataDirty(Data);
                FltReleaseFileNameInformation(FileNameInformation);
                FltReleaseFileNameInformation(DestinationFileNameInformation);
                return FLT_PREOP_COMPLETE;
            }
        }
    }
    FltReleaseFileNameInformation(FileNameInformation);
}