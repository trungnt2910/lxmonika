#pragma once

#include <ntifs.h>

// thread.h
//
// Thread support functions

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _PS_PICO_SYSTEM_CALL_INFORMATION *PPS_PICO_SYSTEM_CALL_INFORMATION;

typedef struct _MX_THREAD {
    ULONG_PTR ReferenceCount;
    PPS_PICO_SYSTEM_CALL_INFORMATION CurrentSystemCall;
} MX_THREAD, *PMX_THREAD;

NTSTATUS
    MxThreadAllocate(
        _Out_ PMX_THREAD* pPMxThread
    );

NTSTATUS
    MxThreadReference(
        _Inout_ PMX_THREAD pMxThread
    );

VOID
    MxThreadFree(
        _Inout_ PMX_THREAD pMxThread
    );

#ifdef __cplusplus
}
#endif
