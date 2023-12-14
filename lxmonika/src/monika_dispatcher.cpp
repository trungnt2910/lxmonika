#include "monika.h"

#include "os.h"

#include "Locker.h"
#include "ProcessMap.h"

//
// Pico provider dispatchers
//

#define MA_LOCK_PROCESS_MAP auto _ = Locker(&MapProcessMap)

#define MA_TRY_DISPATCH_TO_PROVIDER(process, function, ...)                                     \
    do                                                                                          \
    {                                                                                           \
                                                                                                \
        BOOLEAN bHasProvider = FALSE;                                                           \
        DWORD dwProvider;                                                                       \
                                                                                                \
        {                                                                                       \
            MA_LOCK_PROCESS_MAP;                                                                \
            if (NT_SUCCESS(MapProcessMap.GetProcessProvider((process), &dwProvider)))           \
            {                                                                                   \
                bHasProvider = TRUE;                                                            \
            }                                                                                   \
        }                                                                                       \
                                                                                                \
        if (bHasProvider)                                                                       \
        {                                                                                       \
            return MapProviderRoutines[dwProvider].function(__VA_ARGS__);                       \
        }                                                                                       \
    } while (false)

extern "C"
VOID
MapSystemCallDispatch(
    _In_ PPS_PICO_SYSTEM_CALL_INFORMATION SystemCall
)
{
    MA_TRY_DISPATCH_TO_PROVIDER(PsGetCurrentProcess(), DispatchSystemCall, SystemCall);

    if (MapPreSyscallHook(SystemCall))
    {
        PS_PICO_SYSTEM_CALL_INFORMATION beforeSystemCall;
        memcpy(&beforeSystemCall, SystemCall, sizeof(beforeSystemCall));

        KTRAP_FRAME beforeTrapFrame;
        memcpy(&beforeTrapFrame, SystemCall->TrapFrame, sizeof(beforeTrapFrame));

        beforeSystemCall.TrapFrame = &beforeTrapFrame;

        MapOriginalProviderRoutines.DispatchSystemCall(SystemCall);

        MapPostSyscallHook(&beforeSystemCall, SystemCall);
    }
}

extern "C"
VOID
MapThreadExit(
    _In_ PETHREAD Thread
)
{
    MA_TRY_DISPATCH_TO_PROVIDER(PsGetThreadProcess(Thread), ExitThread, Thread);

    return MapOriginalProviderRoutines.ExitThread(Thread);
}

extern "C"
VOID
MapProcessExit(
    _In_ PEPROCESS Process
)
{
    PROCESS_PROVIDER_INFORMATION info{ .Value = PROCESS_PROVIDER_NULL };

    {
        MA_LOCK_PROCESS_MAP;
        if (NT_SUCCESS(MapProcessMap.GetProcessProvider(Process, &info)))
        {
            MapProcessMap.UnregisterProcess(Process);
        }
    }

    if (info.HasProvider)
    {
        MapProviderRoutines[info.ProviderId].ExitProcess(Process);
    }

    if (info.HasProvider && info.HasInternalParentProvider)
    {
        MapProviderRoutines[info.ParentProviderId].ExitProcess(Process);
    }

    if (!info.HasProvider || !info.HasInternalParentProvider)
    {
        MapOriginalProviderRoutines.ExitProcess(Process);
    }
}

extern "C"
BOOLEAN
MapDispatchException(
    _Inout_ PEXCEPTION_RECORD ExceptionRecord,
    _Inout_ PKEXCEPTION_FRAME ExceptionFrame,
    _Inout_ PKTRAP_FRAME TrapFrame,
    _In_ ULONG Chance,
    _In_ KPROCESSOR_MODE PreviousMode
)
{
    MA_TRY_DISPATCH_TO_PROVIDER(PsGetCurrentProcess(), DispatchException,
        ExceptionRecord,
        ExceptionFrame,
        TrapFrame,
        Chance,
        PreviousMode
    );

    return MapOriginalProviderRoutines.DispatchException(
        ExceptionRecord,
        ExceptionFrame,
        TrapFrame,
        Chance,
        PreviousMode
    );
}

extern "C"
NTSTATUS
MapTerminateProcess(
    _In_ PEPROCESS Process,
    _In_ NTSTATUS TerminateStatus
)
{
    MA_TRY_DISPATCH_TO_PROVIDER(Process, TerminateProcess, Process, TerminateStatus);

    return MapOriginalProviderRoutines.TerminateProcess(
        Process,
        TerminateStatus
    );
}

extern "C"
_Ret_range_(<= , FrameCount)
ULONG
MapWalkUserStack(
    _In_ PKTRAP_FRAME TrapFrame,
    _Out_writes_to_(FrameCount, return) PVOID* Callers,
    _In_ ULONG FrameCount
)
{
    MA_TRY_DISPATCH_TO_PROVIDER(PsGetCurrentProcess(), WalkUserStack,
        TrapFrame,
        Callers,
        FrameCount
    );

    return MapOriginalProviderRoutines.WalkUserStack(
        TrapFrame,
        Callers,
        FrameCount
    );
}

//
// Pico routine dispatchers
//

#define MA_PROCESS_BELONGS_TO_PROVIDER(process, index)                                          \
    (([](PEPROCESS p, DWORD i)                                                                  \
    {                                                                                           \
        MA_LOCK_PROCESS_MAP;                                                                    \
        return MapProcessMap.ProcessBelongsToProvider(p, i);                                    \
    })((process), (index)))

static
NTSTATUS
MapCreateProcess(
    _In_ DWORD ProviderIndex,
    _In_ PPS_PICO_PROCESS_ATTRIBUTES ProcessAttributes,
    _Outptr_ PHANDLE ProcessHandle
)
{
    NTSTATUS status = MapOriginalRoutines.CreateProcess(ProcessAttributes, ProcessHandle);

    if (NT_SUCCESS(status))
    {
        PEPROCESS pNewProcess = NULL;
        if (!NT_SUCCESS(PsLookupProcessByProcessId(ProcessHandle, &pNewProcess)))
        {
            // Should bugcheck here.
            DbgBreakPoint();
        }

        {
            MA_LOCK_PROCESS_MAP;
            status = MapProcessMap.RegisterProcessProvider(pNewProcess, ProviderIndex);
        }

        if (!NT_SUCCESS(status))
        {
            MapOriginalRoutines.TerminateProcess(pNewProcess, status);
        }

        ObDereferenceObject(pNewProcess);
    }

    return status;
}

static
NTSTATUS
MapCreateThread(
    _In_ DWORD ProviderIndex,
    _In_ PPS_PICO_THREAD_ATTRIBUTES ThreadAttributes,
    _Outptr_ PHANDLE ThreadHandle
)
{
    if (ThreadAttributes == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    BOOLEAN bBelongsToProvider = FALSE;

    {
        PEPROCESS pHostProcess = NULL;
        NTSTATUS status = PsLookupProcessByProcessId(ThreadAttributes->Process, &pHostProcess);

        if (!NT_SUCCESS(status))
        {
            return status;
        }

        bBelongsToProvider = MA_PROCESS_BELONGS_TO_PROVIDER(pHostProcess, ProviderIndex);

        ObDereferenceObject(pHostProcess);
    }

    if (!bBelongsToProvider)
    {
        return STATUS_INVALID_PARAMETER;
    }

    return MapOriginalRoutines.CreateThread(ThreadAttributes, ThreadHandle);
}

static
PVOID
MapGetProcessContext(
    _In_ DWORD ProviderIndex,
    _In_ PEPROCESS Process
)
{
    if (!MA_PROCESS_BELONGS_TO_PROVIDER(Process, ProviderIndex))
    {
        // TODO: Return what on error?
        return NULL;
    }

    return MapOriginalRoutines.GetProcessContext(Process);
}

static
PVOID
MapGetThreadContext(
    _In_ DWORD ProviderIndex,
    _In_ PETHREAD Thread
)
{
    if (!MA_PROCESS_BELONGS_TO_PROVIDER(PsGetThreadProcess(Thread), ProviderIndex))
    {
        // TODO: Return what on error?
        return NULL;
    }

    return MapOriginalRoutines.GetThreadContext(Thread);
}

static
VOID
MapSetThreadDescriptorBase(
    _In_ DWORD ProviderIndex,
    _In_ PS_PICO_THREAD_DESCRIPTOR_TYPE Type,
    _In_ ULONG_PTR Base
)
{
#if !DBG
    UNREFERENCED_PARAMETER(ProviderIndex);
#endif
    ASSERT(MA_PROCESS_BELONGS_TO_PROVIDER(PsGetCurrentProcess(), ProviderIndex));

    return MapOriginalRoutines.SetThreadDescriptorBase(Type, Base);
}

static
NTSTATUS
MapTerminateProcess(
    _In_ DWORD ProviderIndex,
    _Inout_ PEPROCESS Process,
    _In_ NTSTATUS ExitStatus
)
{
    if (!MA_PROCESS_BELONGS_TO_PROVIDER(Process, ProviderIndex))
    {
        return STATUS_INVALID_PARAMETER;
    }

    // TODO: Why __inout for the Process parameter?
    return MapOriginalRoutines.TerminateProcess(Process, ExitStatus);
}

static
NTSTATUS
MapSetContextThreadInternal(
    _In_ DWORD ProviderIndex,
    _In_ PETHREAD Thread,
    _In_ PCONTEXT ThreadContext,
    _In_ KPROCESSOR_MODE ProbeMode,
    _In_ KPROCESSOR_MODE CtxMode,
    _In_ BOOLEAN PerformUnwind
)
{
    if (!MA_PROCESS_BELONGS_TO_PROVIDER(PsGetThreadProcess(Thread), ProviderIndex))
    {
        return STATUS_INVALID_PARAMETER;
    }

    return MapOriginalRoutines.SetContextThreadInternal(
        Thread, ThreadContext, ProbeMode, CtxMode, PerformUnwind);
}

static
NTSTATUS
MapGetContextThreadInternal(
    _In_ DWORD ProviderIndex,
    _In_ PETHREAD Thread,
    _Inout_ PCONTEXT ThreadContext,
    _In_ KPROCESSOR_MODE ProbeMode,
    _In_ KPROCESSOR_MODE CtxMode,
    _In_ BOOLEAN PerformUnwind
)
{
    if (!MA_PROCESS_BELONGS_TO_PROVIDER(PsGetThreadProcess(Thread), ProviderIndex))
    {
        return STATUS_INVALID_PARAMETER;
    }

    return MapOriginalRoutines.GetContextThreadInternal(
        Thread, ThreadContext, ProbeMode, CtxMode, PerformUnwind);
}

static
NTSTATUS
MapTerminateThread(
    _In_ DWORD ProviderIndex,
    _Inout_ PETHREAD Thread,
    _In_ NTSTATUS ExitStatus,
    _In_ BOOLEAN DirectTerminate
)
{
    if (!MA_PROCESS_BELONGS_TO_PROVIDER(PsGetThreadProcess(Thread), ProviderIndex))
    {
        return STATUS_INVALID_PARAMETER;
    }

    return MapOriginalRoutines.TerminateThread(Thread, ExitStatus, DirectTerminate);
}

static
NTSTATUS
MapSuspendThread(
    _In_ DWORD ProviderIndex,
    _Inout_ PETHREAD Thread,
    _Out_opt_ PULONG PreviousSuspendCount
)
{
    if (!MA_PROCESS_BELONGS_TO_PROVIDER(PsGetThreadProcess(Thread), ProviderIndex))
    {
        return STATUS_INVALID_PARAMETER;
    }

    return MapOriginalRoutines.SuspendThread(Thread, PreviousSuspendCount);
}

static
NTSTATUS
MapResumeThread(
    _In_ DWORD ProviderIndex,
    _Inout_ PETHREAD Thread,
    _Out_opt_ PULONG PreviousSuspendCount
)
{
    if (!MA_PROCESS_BELONGS_TO_PROVIDER(PsGetThreadProcess(Thread), ProviderIndex))
    {
        return STATUS_INVALID_PARAMETER;
    }

    return MapOriginalRoutines.ResumeThread(Thread, PreviousSuspendCount);
}

#define MONIKA_PROVIDER(index)                                                                  \
    extern "C"                                                                                  \
    NTSTATUS                                                                                    \
        MaPicoCreateProcess##index(                                                             \
            _In_ PPS_PICO_PROCESS_ATTRIBUTES ProcessAttributes,                                 \
            _Outptr_ PHANDLE ProcessHandle                                                      \
        )                                                                                       \
    {                                                                                           \
        return MapCreateProcess(index, ProcessAttributes, ProcessHandle);                       \
    }                                                                                           \
                                                                                                \
    extern "C"                                                                                  \
    NTSTATUS                                                                                    \
        MaPicoCreateThread##index(                                                              \
            _In_ PPS_PICO_THREAD_ATTRIBUTES ThreadAttributes,                                   \
            _Outptr_ PHANDLE ThreadHandle                                                       \
        )                                                                                       \
    {                                                                                           \
        return MapCreateThread(index, ThreadAttributes, ThreadHandle);                          \
    }                                                                                           \
                                                                                                \
    extern "C"                                                                                  \
    PVOID                                                                                       \
        MaPicoGetProcessContext##index(                                                         \
            _In_ PEPROCESS Process                                                              \
        )                                                                                       \
    {                                                                                           \
        return MapGetProcessContext(index, Process);                                            \
    }                                                                                           \
                                                                                                \
    extern "C"                                                                                  \
    PVOID                                                                                       \
        MaPicoGetThreadContext##index(                                                          \
            _In_ PETHREAD Thread                                                                \
        )                                                                                       \
    {                                                                                           \
        return MapGetThreadContext(index, Thread);                                              \
    }                                                                                           \
                                                                                                \
    extern "C"                                                                                  \
    VOID                                                                                        \
        MaPicoSetThreadDescriptorBase##index(                                                   \
            _In_ PS_PICO_THREAD_DESCRIPTOR_TYPE Type,                                           \
            _In_ ULONG_PTR Base                                                                 \
        )                                                                                       \
    {                                                                                           \
        return MapSetThreadDescriptorBase(index, Type, Base);                                   \
    }                                                                                           \
                                                                                                \
    extern "C"                                                                                  \
    NTSTATUS                                                                                    \
        MaPicoTerminateProcess##index(                                                          \
            __inout PEPROCESS Process,                                                          \
            __in NTSTATUS ExitStatus                                                            \
        )                                                                                       \
    {                                                                                           \
        return MapTerminateProcess(index, Process, ExitStatus);                                 \
    }                                                                                           \
                                                                                                \
    extern "C"                                                                                  \
    NTSTATUS                                                                                    \
        MaPicoSetContextThreadInternal##index(                                                  \
            __in PETHREAD Thread,                                                               \
            __in PCONTEXT ThreadContext,                                                        \
            __in KPROCESSOR_MODE ProbeMode,                                                     \
            __in KPROCESSOR_MODE CtxMode,                                                       \
            __in BOOLEAN PerformUnwind                                                          \
        )                                                                                       \
    {                                                                                           \
        return MapSetContextThreadInternal(index,                                               \
            Thread, ThreadContext, ProbeMode, CtxMode, PerformUnwind);                          \
    }                                                                                           \
                                                                                                \
    extern "C"                                                                                  \
    NTSTATUS                                                                                    \
        MaPicoGetContextThreadInternal##index(                                                  \
            __in PETHREAD Thread,                                                               \
            __inout PCONTEXT ThreadContext,                                                     \
            __in KPROCESSOR_MODE ProbeMode,                                                     \
            __in KPROCESSOR_MODE CtxMode,                                                       \
            __in BOOLEAN PerformUnwind                                                          \
        )                                                                                       \
    {                                                                                           \
        return MapGetContextThreadInternal(index,                                               \
            Thread, ThreadContext, ProbeMode, CtxMode, PerformUnwind);                          \
    }                                                                                           \
                                                                                                \
    extern "C"                                                                                  \
    NTSTATUS                                                                                    \
        MaPicoTerminateThread##index(                                                           \
            __inout PETHREAD Thread,                                                            \
            __in NTSTATUS ExitStatus,                                                           \
            __in BOOLEAN DirectTerminate                                                        \
        )                                                                                       \
    {                                                                                           \
        return MapTerminateThread(index, Thread, ExitStatus, DirectTerminate);                  \
    }                                                                                           \
                                                                                                \
    extern "C"                                                                                  \
    NTSTATUS                                                                                    \
        MaPicoSuspendThread##index(                                                             \
            _In_ PETHREAD Thread,                                                               \
            _Out_opt_ PULONG PreviousSuspendCount                                               \
        )                                                                                       \
    {                                                                                           \
        return MapSuspendThread(index, Thread, PreviousSuspendCount);                           \
    }                                                                                           \
                                                                                                \
    extern "C"                                                                                  \
    NTSTATUS                                                                                    \
        MaPicoResumeThread##index(                                                              \
            _In_ PETHREAD Thread,                                                               \
            _Out_opt_ PULONG PreviousSuspendCount                                               \
        )                                                                                       \
    {                                                                                           \
        return MapResumeThread(index, Thread, PreviousSuspendCount);                            \
    }
#include "monika_providers.cpp"
#undef MONIKA_PROVIDER
