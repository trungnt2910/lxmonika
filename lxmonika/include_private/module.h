#pragma once

#include <ntifs.h>

#include "winresource.h"

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

NTSTATUS
    MdlpGetProductVersion(
        _In_ HANDLE hModule,
        _Out_ PCWSTR* pPProductVersion,
        _Inout_opt_ PSIZE_T puSize
    );

// MdlpPatchImport
//
// Patches the target PE module's Import Address Table.
NTSTATUS
    MdlpPatchImport(
        _In_ HANDLE hModule,
        _In_ PCSTR pImportImageName,
        _In_ PCSTR pImportName,
        _Inout_ PVOID* pImportValue
    );

NTSTATUS
    MdlpPatchTrampoline(
        _In_ PVOID pOriginal,
        _In_opt_ PVOID pHook,
        _Inout_ PCHAR pBytes,
        _In_ SIZE_T szBytes
    );

#if defined(_M_X64)
#define MDL_TRAMPOLINE_SIZE     12
#elif defined(_M_ARM64)
#define MDL_TRAMPOLINE_SIZE     16
#elif defined(_M_IX86)
#define MDL_TRAMPOLINE_SIZE     7
#elif defined(_M_ARM)
#define MDL_TRAMPOLINE_SIZE     8
#else
#error Define Trampoline size!
#endif

#ifdef __cplusplus
}
#endif
