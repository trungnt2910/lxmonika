#pragma once

// monika.h
//
// Just Monika.

#include <ntddk.h>

#include "pico.h"

#ifdef MONIKA_IN_DRIVER
#define MONIKA_EXPORT __declspec(dllexport)
#else
#define MONIKA_EXPORT __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

MONIKA_EXPORT
NTSTATUS NTAPI
    MaRegisterPicoProvider(
        _In_ PPS_PICO_PROVIDER_ROUTINES ProviderRoutines,
        _Inout_ PPS_PICO_ROUTINES PicoRoutines
    );

#ifdef MONIKA_IN_DRIVER

//
// Monika private definitions
//

NTSTATUS
    MapInitialize();

VOID
    MapCleanup();

BOOLEAN
    MapPreSyscallHook(
        _In_ PPS_PICO_SYSTEM_CALL_INFORMATION pSyscallInfo
    );

VOID
    MapPostSyscallHook(
        _In_ PPS_PICO_SYSTEM_CALL_INFORMATION pPreviousSyscallInfo,
        _In_ PPS_PICO_SYSTEM_CALL_INFORMATION pCurrentSyscallInfo
    );

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

enum
{
#define MONIKA_PROVIDER(index) MaPicoProvider##index = index,
#include "monika_providers.cpp"
#undef MONIKA_PROVIDER
    MaPicoProviderMaxCount
};

#define MONIKA_PROVIDER(index)                                                          \
    extern PS_PICO_CREATE_PROCESS               MaPicoCreateProcess##index;             \
    extern PS_PICO_CREATE_THREAD                MaPicoCreateThread##index;              \
    extern PS_PICO_GET_PROCESS_CONTEXT          MaPicoGetProcessContext##index;         \
    extern PS_PICO_GET_THREAD_CONTEXT           MaPicoGetThreadContext##index;          \
    extern PS_PICO_SET_THREAD_DESCRIPTOR_BASE   MaPicoSetThreadDescriptorBase##index;   \
    extern PS_PICO_TERMINATE_PROCESS            MaPicoTerminateProcess##index;          \
    extern PS_SET_CONTEXT_THREAD_INTERNAL       MaPicoSetContextThreadInternal##index;  \
    extern PS_GET_CONTEXT_THREAD_INTERNAL       MaPicoGetContextThreadInternal##index;  \
    extern PS_TERMINATE_THREAD                  MaPicoTerminateThread##index;           \
    extern PS_SUSPEND_THREAD                    MaPicoSuspendThread##index;             \
    extern PS_RESUME_THREAD                     MaPicoResumeThread##index;
#include "monika_providers.cpp"
#undef MONIKA_PROVIDER

//
// Monika data
//

extern PS_PICO_PROVIDER_ROUTINES MapOriginalProviderRoutines;
extern PS_PICO_ROUTINES MapOriginalRoutines;

extern PS_PICO_PROVIDER_ROUTINES MapProviderRoutines[MaPicoProviderMaxCount];
extern PS_PICO_ROUTINES MapRoutines[MaPicoProviderMaxCount];

class ProcessMap;
extern ProcessMap MapProcessMap;

#endif // MONIKA_IN_DRIVER

#ifdef __cplusplus
}
#endif
