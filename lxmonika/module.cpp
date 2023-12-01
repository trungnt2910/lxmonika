#include "module.h"

#include "pe.h"
#include "os.h"

#include "PoolAllocator.h"

extern "C"
NTSTATUS
MdlpFindModuleByName(
    _In_ PCSTR pModuleName,
    _Out_ PHANDLE pHandle,
    _Out_opt_ PSIZE_T puSize
)
{
    NTSTATUS status;

    while (true)
    {
        ULONG uLen = 0;

        // Query the array size
        // As we do not know the size yet, it will return STATUS_INFO_LENGTH_MISMATCH.
        ZwQuerySystemInformation(SystemModuleInformation, (PVOID)&uLen, 0, &uLen);

        PoolAllocator pModulesAllocator(uLen, '--xS');
        if (pModulesAllocator.Get() == NULL)
        {
            return STATUS_NO_MEMORY;
        }

        PRTL_PROCESS_MODULES pModules = pModulesAllocator.Get<RTL_PROCESS_MODULES>();

        status = ZwQuerySystemInformation(SystemModuleInformation, (PVOID)pModules, uLen, &uLen);
        if (!NT_SUCCESS(status))
        {
            if (STATUS_INFO_LENGTH_MISMATCH == status)
            {
                continue;
            }
            return status;
        }

        for (ULONG i = 0; i < pModules->NumberOfModules; i++)
        {
            PCSTR pPath = (PCSTR)&pModules->Modules[i].FullPathName;
            SIZE_T uPathLength = strnlen(pPath, sizeof(pModules->Modules[i].FullPathName));

            SSIZE_T sLastComponent = (SSIZE_T)uPathLength;

            while (sLastComponent >= 0 && pPath[sLastComponent] != '\\')
            {
                --sLastComponent;
            }

            if (_stricmp(pPath + sLastComponent + 1, pModuleName) != 0)
            {
                continue;
            }

            if (puSize != NULL)
            {
                *puSize = pModules->Modules[i].ImageSize;
            }
            *pHandle = pModules->Modules[i].ImageBase;
            return STATUS_SUCCESS;
        }

        return STATUS_NOT_FOUND;
    }
}

extern "C"
NTSTATUS
MdlpFindModuleSectionByName(
    _In_ HANDLE hdl,
    _In_ PCSTR pSectionName,
    _Out_ PVOID* pSection,
    _Inout_opt_ PSIZE_T puSize
)
{
    if (hdl == NULL || pSectionName == NULL || pSection == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PCHAR pStart = (PCHAR)hdl;
    PCHAR pEnd = (PCHAR)-1;

    if (puSize != NULL && *puSize != 0)
    {
        pEnd = pStart + *puSize;
    }

    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pStart;
    PIMAGE_NT_HEADERS pPeHeader = (PIMAGE_NT_HEADERS)(pStart + pDosHeader->e_lfanew);

    // Check if the end of the PE header is in the module range.
    if ((PCHAR)&pPeHeader[1] > pEnd)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // The section headers array comes right after the PE header.
    PIMAGE_SECTION_HEADER pSectionHeaders = (PIMAGE_SECTION_HEADER)(&pPeHeader[1]);

    // Check if the section headers are in range.
    if ((PCHAR)&pSectionHeaders[pPeHeader->FileHeader.NumberOfSections] > pEnd)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PIMAGE_SECTION_HEADER pInterestedSectionHeader = NULL;

    for (SIZE_T i = 0; i < pPeHeader->FileHeader.NumberOfSections; ++i)
    {
        if (strncmp(pSectionName, (const char*)pSectionHeaders[i].Name,
            IMAGE_SIZEOF_SHORT_NAME) == 0)
        {
            pInterestedSectionHeader = &pSectionHeaders[i];
        }
    }

    if (pInterestedSectionHeader == NULL)
    {
        return STATUS_NOT_FOUND;
    }

    if (pStart + pInterestedSectionHeader->VirtualAddress
        + pInterestedSectionHeader->Misc.VirtualSize > pEnd)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (puSize != NULL)
    {
        *puSize = pInterestedSectionHeader->Misc.VirtualSize;
    }

    *pSection = pStart + pInterestedSectionHeader->VirtualAddress;
    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
MdlpGetProcAddress(
    _In_ HANDLE hModule,
    _In_ PCSTR  lpProcName,
    _Out_ PVOID* pProc
)
{
    if (hModule == NULL || lpProcName == NULL || pProc == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PCHAR pStart = (PCHAR)hModule;

    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pStart;
    PIMAGE_NT_HEADERS pPeHeader = (PIMAGE_NT_HEADERS)(pStart + pDosHeader->e_lfanew);

    PIMAGE_EXPORT_DIRECTORY pExportDirectory = (PIMAGE_EXPORT_DIRECTORY)(pStart +
        pPeHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

    SIZE_T uNameCount = pExportDirectory->NumberOfNames;
    PDWORD32 pNameList = (PDWORD32)(pStart + pExportDirectory->AddressOfNames);
    PDWORD32 pFuncList = (PDWORD32)(pStart + pExportDirectory->AddressOfFunctions);
    WORD* pOrdinalList = (WORD*)(pStart + pExportDirectory->AddressOfNameOrdinals);

    for (SIZE_T i = 0; i < uNameCount; ++i)
    {
        if (strcmp(lpProcName, (PCSTR)(pStart + pNameList[i])) == 0)
        {
            *pProc = (PVOID)(pStart + pFuncList[pOrdinalList[i]]);
            return STATUS_SUCCESS;
        }
    }

    return STATUS_NOT_FOUND;
}
