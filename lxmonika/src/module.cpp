#include "module.h"

#include "pe.h"
#include "os.h"
#include "winresource.h"

#include "Logger.h"
#include "PoolAllocator.h"

#define MDL_RETURN_IF_OUT_OF_BOUNDS(ptr, error) \
    do                                          \
    {                                           \
        if ((PCHAR)(ptr) > (PCHAR)(pEnd))       \
            return (error);                     \
    }                                           \
    while (FALSE);

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
    MDL_RETURN_IF_OUT_OF_BOUNDS(&pPeHeader[1], STATUS_INVALID_PARAMETER);

    // The section headers array comes right after the PE header.
    PIMAGE_SECTION_HEADER pSectionHeaders = (PIMAGE_SECTION_HEADER)(&pPeHeader[1]);

    // Check if the section headers are in range.
    MDL_RETURN_IF_OUT_OF_BOUNDS(&pSectionHeaders[pPeHeader->FileHeader.NumberOfSections],
        STATUS_INVALID_PARAMETER);

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

    MDL_RETURN_IF_OUT_OF_BOUNDS(pStart + pInterestedSectionHeader->VirtualAddress
                                + pInterestedSectionHeader->Misc.VirtualSize,
        STATUS_INVALID_PARAMETER);

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

static
NTSTATUS
MdlpGetResourceDirectory(
    _In_ HANDLE hModule,
    _In_ SIZE_T uModuleSize,
    _In_ LPSTR pIntResource,
    _In_ WORD wId,
    _Out_ PVOID* pPRsrcSectionStart,
    _Out_ PIMAGE_RESOURCE_DIRECTORY* pPResourceDirectory
)
{
    PCHAR pStart = (PCHAR)hModule;
    PCHAR pEnd = pStart + uModuleSize;

    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pStart;
    PIMAGE_NT_HEADERS pPeHeader = (PIMAGE_NT_HEADERS)(pStart + pDosHeader->e_lfanew);
    MDL_RETURN_IF_OUT_OF_BOUNDS(pPeHeader, STATUS_INVALID_PARAMETER);

    PIMAGE_RESOURCE_DIRECTORY pResourceDirectory = (PIMAGE_RESOURCE_DIRECTORY)(pStart +
        pPeHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress);
    MDL_RETURN_IF_OUT_OF_BOUNDS(pResourceDirectory, STATUS_INVALID_PARAMETER);

    // For calculating offsets.
    PCHAR pRsrcSectionStart = (PCHAR)pResourceDirectory;

    // The entries come right after the directory.
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pEntryList = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)
        (pResourceDirectory + 1);
    MDL_RETURN_IF_OUT_OF_BOUNDS(pEntryList, STATUS_INVALID_PARAMETER);

    SIZE_T uTotalEntries = pResourceDirectory->NumberOfIdEntries
        + pResourceDirectory->NumberOfNamedEntries;
    MDL_RETURN_IF_OUT_OF_BOUNDS(&pEntryList[uTotalEntries], STATUS_INVALID_PARAMETER);

    WORD pResourcePath[] =
    {
        (WORD)(ULONG_PTR)pIntResource,
        wId
    };

    const auto GoDownPath = [&](PIMAGE_RESOURCE_DIRECTORY_ENTRY pEntry)
    {
        pResourceDirectory = (PIMAGE_RESOURCE_DIRECTORY)
            (pRsrcSectionStart + pEntry->OffsetToDirectory);
        MDL_RETURN_IF_OUT_OF_BOUNDS(pResourceDirectory, STATUS_INVALID_PARAMETER);

        pEntryList = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(pResourceDirectory + 1);
        MDL_RETURN_IF_OUT_OF_BOUNDS(pEntryList, STATUS_INVALID_PARAMETER);

        uTotalEntries = pResourceDirectory->NumberOfIdEntries
            + pResourceDirectory->NumberOfNamedEntries;
        MDL_RETURN_IF_OUT_OF_BOUNDS(&pEntryList[uTotalEntries], STATUS_INVALID_PARAMETER);

        return STATUS_SUCCESS;
    };

    for (SIZE_T i = 0; i < ARRAYSIZE(pResourcePath); ++i)
    {
        for (SIZE_T j = 0; j < uTotalEntries; ++j)
        {
            if (!pEntryList[j].NameIsString
                && pEntryList[j].Id == pResourcePath[i]
                && pEntryList[j].DataIsDirectory)
            {
                // Go down to the tree on this path.
                NTSTATUS status = GoDownPath(&pEntryList[j]);

                if (!NT_SUCCESS(status))
                {
                    return status;
                }

                Logger::LogTrace("Found level ", i + 1, " directory with ", uTotalEntries,
                    " entries.");

                goto found;
            }
        }

        Logger::LogError("Cannot find level ", i + 1, " directory with ID ", pResourcePath[i]);
        return STATUS_RESOURCE_TYPE_NOT_FOUND;

    found:
        continue;
    }

    *pPRsrcSectionStart = pRsrcSectionStart;
    *pPResourceDirectory = pResourceDirectory;

    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
MdlpGetProductVersion(
    _In_ HANDLE hModule,
    _Out_ PCWSTR* pPProductVersion,
    _Inout_opt_ PSIZE_T puSize
)
{
    if (hModule == NULL || pPProductVersion == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PCHAR pStart = (PCHAR)hModule;
    PCHAR pEnd = (PCHAR)-1;

    if (puSize != NULL && *puSize != 0)
    {
        pEnd = pStart + *puSize;
    }

    PIMAGE_RESOURCE_DIRECTORY pResourceDirectory;
    PCHAR pRsrcSectionStart;

    NTSTATUS status = MdlpGetResourceDirectory(
        hModule,
        pEnd - pStart,
        RT_VERSION,
        // This param must be 1
        // https://learn.microsoft.com/en-us/windows/win32/menurc/versioninfo-resource
        1,
        (PVOID*)&pRsrcSectionStart,
        &pResourceDirectory
    );

    if (!NT_SUCCESS(status))
    {
        Logger::LogError("Cannot find resource directory. Maybe the OS has already unloaded it?");
        return status;
    }

    MDL_RETURN_IF_OUT_OF_BOUNDS(pResourceDirectory, STATUS_INVALID_PARAMETER);
    MDL_RETURN_IF_OUT_OF_BOUNDS(pRsrcSectionStart, STATUS_INVALID_PARAMETER);

    // The entries come right after the directory.
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pEntryList = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)
        (pResourceDirectory + 1);
    MDL_RETURN_IF_OUT_OF_BOUNDS(pEntryList, STATUS_INVALID_PARAMETER);

    SIZE_T uTotalEntries = pResourceDirectory->NumberOfIdEntries
        + pResourceDirectory->NumberOfNamedEntries;
    MDL_RETURN_IF_OUT_OF_BOUNDS(&pEntryList[uTotalEntries], STATUS_INVALID_PARAMETER);

    // Take the first directory type entry. Should be a leaf of the VS_VERSIONINFO data type.
    // https://learn.microsoft.com/en-us/windows/win32/menurc/vs-versioninfo
    for (SIZE_T i = 0; i < uTotalEntries; ++i)
    {
        if (!pEntryList[i].DataIsDirectory)
        {
            PIMAGE_RESOURCE_DATA_ENTRY pDataEntry = (PIMAGE_RESOURCE_DATA_ENTRY)
                (pRsrcSectionStart + pEntryList[i].OffsetToData);
            MDL_RETURN_IF_OUT_OF_BOUNDS(pDataEntry, STATUS_INVALID_PARAMETER);

            SIZE_T uVersionInfoLength = pDataEntry->Size;
            PCHAR pVersionInfoStart = pStart + pDataEntry->OffsetToData;
            MDL_RETURN_IF_OUT_OF_BOUNDS(pVersionInfoStart, STATUS_INVALID_PARAMETER);

            PCHAR pProductVersion = pVersionInfoStart;

            // TODO: Implement actual resource string tree parsing.
            const WCHAR pPattern[] = L"ProductVersion";
            while (pProductVersion + sizeof(pPattern)
                   <= pVersionInfoStart + uVersionInfoLength)
            {
                if (wcsncmp((PCWSTR)pProductVersion, pPattern, sizeof(pPattern)) != 0)
                {
                    ++pProductVersion;
                    continue;
                }

                goto found_product_version;
            }

            Logger::LogError("Cannot find product version");
            return STATUS_NOT_FOUND;

        found_product_version:
            // Align up a 32-bit boundary.
            PWCHAR pProductVersionValue = (PWCHAR)ALIGN_UP_BY(
                pProductVersion + sizeof(pPattern), 4);

            *pPProductVersion = pProductVersionValue;

            if (puSize != NULL)
            {
                *puSize = wcslen(pProductVersionValue) * sizeof(WCHAR);
            }

            return STATUS_SUCCESS;
        }
    }

    // No languages available?
    Logger::LogError("Cannot find leaf");
    return STATUS_RESOURCE_LANG_NOT_FOUND;
}
