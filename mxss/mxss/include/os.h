#pragma once

#include <ntddk.h>

// os.h
//
// Support definitions for Windows NT OS internal functions.

#pragma warning(disable: 4201)

#ifdef __cplusplus
extern "C"
{
#endif

NTSTATUS
NTSYSAPI NTAPI
    ZwProtectVirtualMemory(
        _In_ HANDLE ProcessHandle,
        _Inout_ PVOID* BaseAddress,
        _Inout_ PSIZE_T NumberOfBytesToProtect,
        _In_ ULONG NewAccessProtection,
        _Out_opt_ PULONG OldAccessProtection
    );

#define MEM_DOS_LIM 0x40000000

#ifdef __cplusplus
}
#endif
