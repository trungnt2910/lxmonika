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

/// <summary>Sets the name of the specified provider.</summary>
///
/// <param name="ProviderDetails">
/// An opaque structure representing the Pico provider. For this version of <c>lxmonika</c>, a
/// pointer to a structure that is equal to the provider routines passed to
/// <see cref="MaRegisterPicoProvider" /> should work.
/// </param>
///
/// <param name="Name">
/// A null-terminated string. If the string exceeds <see cref="MA_NAME_MAX" /> characters (not
/// including the null terminator), it will be truncated when registered. The name is recommended
/// (but not required) to have the following format:
/// <c>"{Kernel Name}-{Kernel Release}-{Other details}"</c>.
/// </param>
///
/// <remarks>
/// The function must be called when the provider has no active processes. Trying to change/set
/// the Pico provider name while hosting processes results in undefined behavior.
/// When a Pico process requests a provider by name, these registered names will be searched.
/// The first registered provider with the longest common prefix with the queried name will be
/// selected.
/// For example, if "Linux-5.15" and "Linux-6.6" are registered in this order, "Linux" would match
/// "Linux-5.15", while "Linux-6" would match "Linux-6.6".
/// </remarks>
MONIKA_EXPORT
NTSTATUS NTAPI
    MaSetPicoProviderName(
        _In_ PVOID ProviderDetails,
        _In_ PCSTR Name
    );

#include "monika_constants.h"

#ifdef MONIKA_IN_DRIVER

#include <intsafe.h>

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

NTSTATUS
    MapInitializeLxssDevice(
        _In_ PDRIVER_OBJECT DriverObject
    );

VOID
    MapCleanupLxssDevice();

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

//
// Monika-managed context
//

#define MA_CONTEXT_MAGIC ('moni')

typedef struct _MA_CONTEXT {
    ULONG                   Magic;
    DWORD                   Provider;
    PVOID                   Context;
    struct _MA_CONTEXT*     Parent;
} MA_CONTEXT, *PMA_CONTEXT;

PMA_CONTEXT
    MapAllocateContext(
        _In_ DWORD Provider,
        _In_opt_ PVOID OriginalContext
    );

VOID
    MapFreeContext(
        _In_ PMA_CONTEXT Context
    );

/// <summary>
/// Sets <paramref name="CurrentContext"/> as the parent of <paramref name="NewContext"/>,
/// then swaps the contents of the two pointed structs.
/// </summary>
NTSTATUS
    MapPushContext(
        _Inout_ PMA_CONTEXT CurrentContext,
        _In_ PMA_CONTEXT NewContext
    );

/// <summary>
/// Sets <paramref name="CurrentContext"/> to the contents of its own parent,
/// then destorys the parent's memory.
/// </summary>
NTSTATUS
    MapPopContext(
        _Inout_ PMA_CONTEXT CurrentContext
    );

//
// Monika provider-specific callbacks
//

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
extern CHAR MapProviderNames[MaPicoProviderMaxCount][MA_NAME_MAX + 1];
extern SIZE_T MapProvidersCount;

extern BOOLEAN MapPatchedLxss;

#endif // MONIKA_IN_DRIVER

#ifdef __cplusplus
}
#endif
