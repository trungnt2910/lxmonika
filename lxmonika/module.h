#pragma once

#include <ntddk.h>

// module.h
// 
// Module support functions.

#ifdef __cplusplus
extern "C"
{
#endif

NTSTATUS
    MdlpFindModuleByName(
        _In_ PCSTR pModuleName,
        _Out_ PHANDLE pHandle,
        _Out_opt_ PSIZE_T puSize
    );

// MdlpFindModuleSectionByName
// 
// Finds the PE section with the name pSectionName of the module specified in hdl.
// 
// The start of the section will be placed in pSection.
// 
// If puSize is not NULL, it will be interpreted as the size of the PE module.
// 0 means the size is not specified.
// 
// If puSize is not NULL, the size of the section will be placed in puSize.
NTSTATUS
    MdlpFindModuleSectionByName(
        _In_ HANDLE hdl,
        _In_ PCSTR pSectionName,
        _Out_ PVOID* pSection,
        _Inout_opt_ PSIZE_T puSize
    );

NTSTATUS
    MdlpGetProcAddress(
        _In_ HANDLE hModule,
        _In_ PCSTR  lpProcName,
        _Out_ PVOID* pProc
    );

#ifdef __cplusplus
}
#endif
