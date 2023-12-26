#pragma once

// provider.h
//
// Pico provider standard functions.

#include <ntifs.h>
#include <pico.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern PS_PICO_ROUTINES MxRoutines;

VOID
    MxSystemCallDispatch(
        _In_ PPS_PICO_SYSTEM_CALL_INFORMATION SystemCall
    );

VOID
    MxThreadExit(
        _In_ PETHREAD Thread
    );

VOID
    MxProcessExit(
        _In_ PEPROCESS Process
    );

BOOLEAN
    MxDispatchException(
        _Inout_ PEXCEPTION_RECORD ExceptionRecord,
        _Inout_ PKEXCEPTION_FRAME ExceptionFrame,
        _Inout_ PKTRAP_FRAME TrapFrame,
        _In_ ULONG Chance,
        _In_ KPROCESSOR_MODE PreviousMode
    );

NTSTATUS
    MxTerminateProcess(
        _In_ PEPROCESS Process,
        _In_ NTSTATUS TerminateStatus
    );

_Ret_range_(<= , FrameCount)
ULONG
    MxWalkUserStack(
        _In_ PKTRAP_FRAME TrapFrame,
        _Out_writes_to_(FrameCount, return) PVOID* Callers,
        _In_ ULONG FrameCount
    );

#ifdef __cplusplus
}
#endif
