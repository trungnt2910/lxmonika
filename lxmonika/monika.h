#pragma once

// monika.h
//
// Just Monika.

#include <ntddk.h>

#include "pico.h"

#ifdef __cplusplus
extern "C"
{
#endif

__declspec(dllexport)
NTSTATUS NTAPI
    MaRegisterPicoProvider(
        _In_ PPS_PICO_PROVIDER_ROUTINES ProviderRoutines,
        _Inout_ PPS_PICO_ROUTINES PicoRoutines
    );

NTSTATUS
    MapInitialize();

VOID
    MapCleanup();

VOID
    MapSystemCallDispatch(
        _In_ PPS_PICO_SYSTEM_CALL_INFORMATION SystemCall
    );

VOID
    MapThreadExit(
        _In_ PETHREAD Thread
    );

VOID
    MapProcessExit(
        _In_ PEPROCESS Process
    );

BOOLEAN
    MapDispatchException(
        _Inout_ PEXCEPTION_RECORD ExceptionRecord,
        _Inout_ PKEXCEPTION_FRAME ExceptionFrame,
        _Inout_ PKTRAP_FRAME TrapFrame,
        _In_ ULONG Chance,
        _In_ KPROCESSOR_MODE PreviousMode
    );

NTSTATUS
    MapTerminateProcess(
        _In_ PEPROCESS Process,
        _In_ NTSTATUS TerminateStatus
    );

_Ret_range_(<= , FrameCount)
ULONG
    MapWalkUserStack(
        _In_ PKTRAP_FRAME TrapFrame,
        _Out_writes_to_(FrameCount, return) PVOID* Callers,
        _In_ ULONG FrameCount
    );

#ifdef __cplusplus
}
#endif
