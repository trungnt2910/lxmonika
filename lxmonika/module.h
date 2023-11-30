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
        IN PCSTR pModuleName,
        OUT PHANDLE pHandle,
        OUT PSIZE_T puSize OPTIONAL
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
        IN HANDLE hdl,
        IN PCSTR pSectionName,
        OUT PVOID pSection,
        PSIZE_T puSize OPTIONAL
    );

#ifdef __cplusplus
}
#endif
