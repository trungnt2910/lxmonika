#pragma once

#include <ntifs.h>

// console.h
//
// Console support functions

#ifdef __cplusplus
extern "C"
{
#endif

NTSTATUS
    CoAttachKernelConsole(
        _In_ HANDLE hdlOwnerProcess,
        _Out_ PHANDLE pHdlConsole
    );

NTSTATUS
    CoOpenStandardHandles(
        _In_ HANDLE hdlConsole,
        _Out_opt_ PHANDLE pHdlInput,
        _Out_opt_ PHANDLE pHdlOutput
    );

DECLSPEC_DEPRECATED
NTSTATUS
    CoOpenNewestWslHandle(
        _Out_ PHANDLE pHdlWsl
    );

#ifdef __cplusplus
}
#endif
