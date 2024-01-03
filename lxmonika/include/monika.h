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

typedef
NTSTATUS
    MA_PICO_GET_ALLOCATED_PROVIDER_NAME(
        _Outptr_ PUNICODE_STRING* ProviderName
    );
typedef MA_PICO_GET_ALLOCATED_PROVIDER_NAME* PMA_PICO_GET_ALLOCATED_PROVIDER_NAME;

typedef struct _MA_PICO_SESSION_ATTRIBUTES {
    SIZE_T Size;
    HANDLE HostProcess;
    HANDLE Console;
    HANDLE Input;
    HANDLE Output;
    HANDLE RootDirectory;
    HANDLE CurrentWorkingDirectory;
    SIZE_T ProviderArgsCount;
    PUNICODE_STRING ProviderArgs;
    SIZE_T ArgsCount;
    PUNICODE_STRING Args;
    SIZE_T EnvironmentCount;
    PUNICODE_STRING Environment;
} MA_PICO_SESSION_ATTRIBUTES, *PMA_PICO_SESSION_ATTRIBUTES;

typedef
NTSTATUS
    MA_PICO_START_SESSION(
        _In_ PMA_PICO_SESSION_ATTRIBUTES Attributes
    );
typedef MA_PICO_START_SESSION* PMA_PICO_START_SESSION;

typedef
NTSTATUS
    MA_PICO_GET_CURRENT_WORKING_DIRECTORY(
        _In_ PEPROCESS Process,
        _Out_ PHANDLE CurrentWorkingDirectory
    );
typedef MA_PICO_GET_CURRENT_WORKING_DIRECTORY* PMA_PICO_GET_CURRENT_WORKING_DIRECTORY;

typedef
NTSTATUS
    MA_PICO_GET_CONSOLE(
        _In_ PEPROCESS Process,
        _Out_opt_ PHANDLE Console,
        _Out_opt_ PHANDLE Input,
        _Out_opt_ PHANDLE Output
    );
typedef MA_PICO_GET_CONSOLE* PMA_PICO_GET_CONSOLE;

typedef struct _MA_PICO_PROVIDER_ROUTINES {
    SIZE_T Size;
    PMA_PICO_GET_ALLOCATED_PROVIDER_NAME GetAllocatedProviderName;
    PMA_PICO_START_SESSION StartSession;
    PMA_PICO_GET_CURRENT_WORKING_DIRECTORY GetCurrentWorkingDirectory;
    PMA_PICO_GET_CONSOLE GetConsole;
} MA_PICO_PROVIDER_ROUTINES, *PMA_PICO_PROVIDER_ROUTINES;

typedef struct _MA_PICO_ROUTINES {
    SIZE_T Size;
} MA_PICO_ROUTINES;
typedef MA_PICO_ROUTINES* PMA_PICO_ROUTINES;

MONIKA_EXPORT
NTSTATUS NTAPI
    MaRegisterPicoProvider(
        _In_ PPS_PICO_PROVIDER_ROUTINES ProviderRoutines,
        _Inout_ PPS_PICO_ROUTINES PicoRoutines
    );

MONIKA_EXPORT
NTSTATUS NTAPI
    MaRegisterPicoProviderEx(
        _In_ PPS_PICO_PROVIDER_ROUTINES ProviderRoutines,
        _Inout_ PPS_PICO_ROUTINES PicoRoutines,
        _In_opt_ PMA_PICO_PROVIDER_ROUTINES AdditionalProviderRoutines,
        _Inout_opt_ PMA_PICO_ROUTINES AdditionalPicoRoutines,
        _Out_opt_ PSIZE_T Index
    );

/// <summary>Sets the name of the specified provider.</summary>
///
/// <param name="Name">
/// A null-terminated Unicode string. The name is recommended (but not required) to have the
/// following format: <c>"{Kernel Name}-{Kernel Release}-{Other details}"</c>.
/// </param>
///
/// <param name="Index">
/// A pointer to receive the found index.
/// </param>
///
/// <remarks>
/// The function looks for providers based on the names provided by the
/// <c>GetAllocatedProviderName</c> extended provider routine.
/// The first provider reporting the name with the longest common prefix with the queried name will
/// be selected.
/// For example, if "Linux-5.15" and "Linux-6.6" are reported in this order, "Linux" would match
/// "Linux-5.15", while "Linux-6" would match "Linux-6.6".
/// </remarks>
MONIKA_EXPORT
NTSTATUS NTAPI
    MaFindPicoProvider(
        _In_ PCWSTR ProviderName,
        _Out_ PSIZE_T Index
    );

MONIKA_EXPORT
NTSTATUS NTAPI
    MaGetAllocatedPicoProviderName(
        _In_ SIZE_T Index,
        _Out_ PUNICODE_STRING* ProviderName
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

VOID
    MapLxssSystemCallHook(
        _In_ PPS_PICO_SYSTEM_CALL_INFORMATION pSyscallInfo
    );

NTSTATUS
    MapLxssGetAllocatedProviderName(
        _Outptr_ PUNICODE_STRING* pOutProviderName
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
extern SIZE_T MapProvidersCount;

extern BOOLEAN MapPatchedLxss;

#endif // MONIKA_IN_DRIVER

#ifdef __cplusplus
}
#endif
