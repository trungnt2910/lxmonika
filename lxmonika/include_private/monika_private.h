#pragma once

// monika_private.h
//
// Just Monika.

#include "pico.h"

#include "Logger.h"

#ifdef __cplusplus
extern "C"
{
#endif

//
// Monika lifetime
//

NTSTATUS
    MapInitialize();

VOID
    MapCleanup();

//
// Monika LXSS hooks
//

NTSTATUS
    MapLxssInitialize(
        _In_ PDRIVER_OBJECT DriverObject
    );

NTSTATUS
    MapLxssPrepareForPatchGuard();

VOID
    MapLxssCleanup();

VOID
    MapLxssSystemCallHook(
        _In_ PPS_PICO_SYSTEM_CALL_INFORMATION pSyscallInfo
    );

NTSTATUS
    MapLxssGetAllocatedProviderName(
        _Outptr_ PUNICODE_STRING* pOutProviderName
    );

NTSTATUS
    MapLxssGetConsole(
        _In_ PEPROCESS Process,
        _Out_opt_ PHANDLE Console,
        _Out_opt_ PHANDLE Input,
        _Out_opt_ PHANDLE Output
    );

//
// Monika Pico provider callbacks
//

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
    extern PS_PICO_CREATE_PROCESS_TH1           MaPicoCreateProcessTh1##index;          \
    extern PS_PICO_CREATE_THREAD                MaPicoCreateThread##index;              \
    extern PS_PICO_CREATE_THREAD_TH1            MaPicoCreateThreadTh1##index;           \
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

extern BOOLEAN MapTooLate;
extern ULONG MapSystemAbiVersion;

extern PS_PICO_PROVIDER_ROUTINES MapOriginalProviderRoutines;
extern PS_PICO_ROUTINES MapOriginalRoutines;

extern BOOLEAN MapPicoRegistrationDisabled;
extern PS_PICO_PROVIDER_ROUTINES MapProviderRoutines[MaPicoProviderMaxCount];
extern PS_PICO_ROUTINES MapRoutines[MaPicoProviderMaxCount];
extern MA_PICO_PROVIDER_ROUTINES MapAdditionalProviderRoutines[MaPicoProviderMaxCount];
extern SIZE_T MapProvidersCount;

extern BOOLEAN MapLxssPatched;
extern BOOLEAN MapLxssRegistering;
extern SIZE_T MapLxssProviderIndex;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

//
// Monika helper macros
//

#define MA_RETURN_IF_FAIL(s)                                                    \
    do                                                                          \
    {                                                                           \
        NTSTATUS status__ = (s);                                                \
        if (!NT_SUCCESS(status__))                                              \
        {                                                                       \
            Logger::LogTrace("Function failed with status ", (void*)status__);  \
            return status__;                                                    \
        }                                                                       \
    }                                                                           \
    while (FALSE)

//
// Monika context helpers
//

namespace MaDetails
{
    template <typename T, typename U>
    struct IsSameType
    {
        static constexpr bool Value = false;
    };

    template <typename T>
    struct IsSameType<T, T>
    {
        static constexpr bool Value = true;
    };

    template <typename T>
    concept KernelProcess = IsSameType<T, PEPROCESS>::Value;

    template <typename T>
    concept KernelThread = IsSameType<T, PETHREAD>::Value;

    template <typename T>
    concept KernelProcessOrThread = KernelProcess<T> || KernelThread<T>;
}

template <MaDetails::KernelProcessOrThread ObjectT>
NTSTATUS
MapGetObjectContext(
    _In_ ObjectT Object,
    _Out_ PMA_CONTEXT* Context
)
{
    PMA_CONTEXT pContext = NULL;

    if constexpr (MaDetails::KernelProcess<ObjectT>)
    {
        pContext = (PMA_CONTEXT)MapOriginalRoutines.GetProcessContext(Object);
    }
    else if constexpr (MaDetails::KernelThread<ObjectT>)
    {
        pContext = (PMA_CONTEXT)MapOriginalRoutines.GetThreadContext(Object);
    }

    if (pContext == NULL || pContext->Magic != MA_CONTEXT_MAGIC)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *Context = pContext;
    return STATUS_SUCCESS;
}

template <MaDetails::KernelProcessOrThread ObjectT>
NTSTATUS
MapGetObjectContext(
    _In_ ObjectT Object,
    _In_ DWORD ProviderIndex,
    _Out_ PMA_CONTEXT* Context
)
{
    PMA_CONTEXT pContext = NULL;
    MA_RETURN_IF_FAIL(MapGetObjectContext(Object, &pContext));

    if (pContext->Provider != ProviderIndex)
    {
        return STATUS_ACCESS_DENIED;
    }

    *Context = pContext;
    return STATUS_SUCCESS;
}

#endif // __cplusplus
