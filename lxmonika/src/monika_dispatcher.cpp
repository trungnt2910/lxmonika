#include "monika.h"

#include "os.h"

#include "AutoResource.h"

//
// Pico provider dispatchers
//

// Mess up these concepts, and the runtime consequences will be severe.
static_assert(MaDetails::KernelProcess<PEPROCESS>, "Concept broken");
static_assert(!MaDetails::KernelProcess<PETHREAD>, "Concept broken");
static_assert(!MaDetails::KernelProcess<PVOID>, "Concept broken");

static_assert(MaDetails::KernelThread<PETHREAD>, "Concept broken");
static_assert(!MaDetails::KernelThread<PEPROCESS>, "Concept broken");
static_assert(!MaDetails::KernelThread<PVOID>, "Concept broken");

static_assert(MaDetails::KernelProcessOrThread<PEPROCESS>, "Concept broken");
static_assert(MaDetails::KernelProcessOrThread<PETHREAD>, "Concept broken");
static_assert(!MaDetails::KernelProcessOrThread<PVOID>, "Concept broken");

#define MA_DISPATCH_FREE            (0x1)
#define MA_DISPATCH_NO_FALLBACK     (0x2)

#define MA_DISPATCH_TO_PROVIDER(type, object, function, ...)                                    \
    do                                                                                          \
    {                                                                                           \
        PMA_CONTEXT pContext = NULL;                                                            \
        if (NT_SUCCESS(MapGetObjectContext(object, &pContext)))                                 \
        {                                                                                       \
            __try                                                                               \
            {                                                                                   \
                return MapProviderRoutines[pContext->Provider].function(__VA_ARGS__);           \
            }                                                                                   \
            __finally                                                                           \
            {                                                                                   \
                if constexpr ((type) & MA_DISPATCH_FREE)                                        \
                {                                                                               \
                    MapFreeContext(pContext);                                                   \
                }                                                                               \
            }                                                                                   \
        }                                                                                       \
        else if constexpr (!((type) & MA_DISPATCH_NO_FALLBACK))                                 \
        {                                                                                       \
            if (MapOriginalProviderRoutines.function != NULL)                                   \
            {                                                                                   \
                return MapOriginalProviderRoutines.function(__VA_ARGS__);                       \
            }                                                                                   \
        }                                                                                       \
    } while (FALSE)

extern "C"
VOID
MapSystemCallDispatch(
    _In_ PPS_PICO_SYSTEM_CALL_INFORMATION SystemCall
)
{
    MA_DISPATCH_TO_PROVIDER(0, PsGetCurrentThread(), DispatchSystemCall, SystemCall);
}

extern "C"
VOID
MapThreadExit(
    _In_ PETHREAD Thread
)
{
    MA_DISPATCH_TO_PROVIDER(MA_DISPATCH_FREE, Thread, ExitThread, Thread);
}

extern "C"
VOID
MapProcessExit(
    _In_ PEPROCESS Process
)
{
    MA_DISPATCH_TO_PROVIDER(MA_DISPATCH_FREE, Process, ExitProcess, Process);
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
    MA_DISPATCH_TO_PROVIDER(MA_DISPATCH_NO_FALLBACK, PsGetCurrentThread(),
        DispatchException,
            ExceptionRecord,
            ExceptionFrame,
            TrapFrame,
            Chance,
            PreviousMode
    );

    // This happens when a process like taskmgr.exe attachs one of its threads to a Pico process.
    // The current thread would not be a Pico thread (and has no Pico context), but NT still calls
    // the Pico exception dispatcher.
    MA_DISPATCH_TO_PROVIDER(0, PsGetCurrentProcess(),
        DispatchException,
            ExceptionRecord,
            ExceptionFrame,
            TrapFrame,
            Chance,
            PreviousMode
    );

    return FALSE;
}

extern "C"
NTSTATUS
MapTerminateProcess(
    _In_ PEPROCESS Process,
    _In_ NTSTATUS TerminateStatus
)
{
    MA_DISPATCH_TO_PROVIDER(0, Process, TerminateProcess, Process, TerminateStatus);

    return STATUS_NOT_IMPLEMENTED;
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
    MA_DISPATCH_TO_PROVIDER(0, PsGetCurrentThread(),
        WalkUserStack,
            TrapFrame,
            Callers,
            FrameCount
    );

    return 0;
}

//
// Pico routine dispatchers
//

#define MA_DISPATCH_TO_KERNEL(object, index, error, function, ...)                              \
    do                                                                                          \
    {                                                                                           \
        PMA_CONTEXT pContext = NULL;                                                            \
        if (NT_SUCCESS(MapGetObjectContext(object, index, &pContext)))                          \
        {                                                                                       \
            return MapOriginalRoutines.function(__VA_ARGS__);                                   \
        }                                                                                       \
        else                                                                                    \
        {                                                                                       \
            return error;                                                                       \
        }                                                                                       \
    } while (FALSE)

static
NTSTATUS
MapCreateProcess(
    _In_ DWORD ProviderIndex,
    _In_ PPS_PICO_PROCESS_ATTRIBUTES ProcessAttributes,
    _In_opt_ PPS_PICO_CREATE_INFO CreateInfo,
    _Outptr_ PHANDLE ProcessHandle
)
{
    if (ProcessAttributes == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PMA_CONTEXT pContext = MapAllocateContext(ProviderIndex, ProcessAttributes->Context);
    if (pContext == NULL)
    {
        return STATUS_NO_MEMORY;
    }

    ProcessAttributes->Context = pContext;

    NTSTATUS status;

    if (MapSystemAbiVersion >= NTDDI_WIN10_RS1)
    {
        status = MapOriginalRoutines.CreateProcess(
            ProcessAttributes, CreateInfo, ProcessHandle);
    }
    else // TH1
    {
        status = ((PS_PICO_CREATE_PROCESS_TH1*)&MapOriginalRoutines.CreateProcess)(
            ProcessAttributes, ProcessHandle);
    }

    ProcessAttributes->Context = pContext->Context;

    if (!NT_SUCCESS(status))
    {
        MapFreeContext(pContext);
    }

    return status;
}

static
NTSTATUS
MapCreateThread(
    _In_ DWORD ProviderIndex,
    _In_ PPS_PICO_THREAD_ATTRIBUTES ThreadAttributes,
    _In_opt_ PPS_PICO_CREATE_INFO CreateInfo,
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
        MA_RETURN_IF_FAIL(ObReferenceObjectByHandle(
            ThreadAttributes->Process,
            0,
            *PsProcessType,
            KernelMode,
            (PVOID*)&pHostProcess,
            0
        ));
        AUTO_RESOURCE(pHostProcess, ObfDereferenceObject);

        PMA_CONTEXT pHostProcessContext = NULL;
        MA_RETURN_IF_FAIL(MapGetObjectContext(pHostProcess, &pHostProcessContext));

        bBelongsToProvider = pHostProcessContext != NULL
            && pHostProcessContext->Provider == ProviderIndex;
    }

    if (!bBelongsToProvider)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PMA_CONTEXT pContext = MapAllocateContext(ProviderIndex, ThreadAttributes->Context);

    if (pContext == NULL)
    {
        return STATUS_NO_MEMORY;
    }

    ThreadAttributes->Context = pContext;

    NTSTATUS status;

    if (MapSystemAbiVersion >= NTDDI_WIN10_RS1)
    {
        status = MapOriginalRoutines.CreateThread(
            ThreadAttributes, CreateInfo, ThreadHandle);
    }
    else // TH1
    {
        status = ((PS_PICO_CREATE_THREAD_TH1*)&MapOriginalRoutines.CreateThread)(
            ThreadAttributes, ThreadHandle);
    }

    ThreadAttributes->Context = pContext->Context;

    if (!NT_SUCCESS(status))
    {
        MapFreeContext(pContext);
    }

    return status;
}

static
PVOID
MapGetProcessContext(
    _In_ DWORD ProviderIndex,
    _In_ PEPROCESS Process
)
{
    PMA_CONTEXT pContext = NULL;
    if (!NT_SUCCESS(MapGetObjectContext(Process, &pContext))
        || pContext->Provider != ProviderIndex)
    {
        return NULL;
    }

    return pContext->Context;
}

static
PVOID
MapGetThreadContext(
    _In_ DWORD ProviderIndex,
    _In_ PETHREAD Thread
)
{
    PMA_CONTEXT pContext = NULL;
    if (!NT_SUCCESS(MapGetObjectContext(Thread, &pContext))
        || pContext->Provider != ProviderIndex)
    {
        return NULL;
    }

    return pContext->Context;
}

static
VOID
MapSetThreadDescriptorBase(
    _In_ DWORD ProviderIndex,
    _In_ PS_PICO_THREAD_DESCRIPTOR_TYPE Type,
    _In_ ULONG_PTR Base
)
{
    MA_DISPATCH_TO_KERNEL(PsGetCurrentThread(), ProviderIndex, (VOID)STATUS_INVALID_PARAMETER,
        SetThreadDescriptorBase, Type, Base);
}

static
NTSTATUS
MapTerminateProcess(
    _In_ DWORD ProviderIndex,
    _Inout_ PEPROCESS Process,
    _In_ NTSTATUS ExitStatus
)
{
    // TODO: Why __inout for the Process parameter?

    PMA_CONTEXT pContext = NULL;
    MA_RETURN_IF_FAIL(MapGetObjectContext(Process, ProviderIndex, &pContext));

    // By terminating a process, the provider agrees that it does not need the process anymore.
    // However, a parent provider might still reference it in some way, for example, Linux's wait4.
    // Therefore, instead of directly telling the kernel to dispose of the process, we transfer
    // control to the parent provider and return as normal.

    if (pContext->Parent != NULL)
    {
        // Replaces the current context structure with the parent's context structure.
        // We need do swap the contents in-place since NT does not allow us to change the context
        // pointer.
        return MapPopContext(pContext);
    }
    else
    {
        // No more known parents waiting for the process.
        // Directly terminate it and wait for MapProcessExit to clean up the context.
        return MapOriginalRoutines.TerminateProcess(Process, ExitStatus);
    }
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
    MA_DISPATCH_TO_KERNEL(Thread, ProviderIndex, STATUS_INVALID_PARAMETER,
        SetContextThreadInternal, Thread, ThreadContext, ProbeMode, CtxMode, PerformUnwind);
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
    MA_DISPATCH_TO_KERNEL(Thread, ProviderIndex, STATUS_INVALID_PARAMETER,
        GetContextThreadInternal, Thread, ThreadContext, ProbeMode, CtxMode, PerformUnwind);
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
    // The same strategy is applied as MapTerminateProcess, see the above function for details.

    PMA_CONTEXT pContext = NULL;
    MA_RETURN_IF_FAIL(MapGetObjectContext(Thread, ProviderIndex, &pContext));

    if (pContext->Parent != NULL)
    {
        return MapPopContext(pContext);
    }
    else
    {
        // No more known parents waiting for the process.
        // Directly terminate it and wait for MapProcessExit to clean up the context.
        return MapOriginalRoutines.TerminateThread(Thread, ExitStatus, DirectTerminate);
    }
}

static
NTSTATUS
MapSuspendThread(
    _In_ DWORD ProviderIndex,
    _Inout_ PETHREAD Thread,
    _Out_opt_ PULONG PreviousSuspendCount
)
{
    MA_DISPATCH_TO_KERNEL(Thread, ProviderIndex, STATUS_INVALID_PARAMETER,
        SuspendThread, Thread, PreviousSuspendCount);
}

static
NTSTATUS
MapResumeThread(
    _In_ DWORD ProviderIndex,
    _Inout_ PETHREAD Thread,
    _Out_opt_ PULONG PreviousSuspendCount
)
{
    MA_DISPATCH_TO_KERNEL(Thread, ProviderIndex, STATUS_INVALID_PARAMETER,
        ResumeThread, Thread, PreviousSuspendCount);
}

#define MONIKA_PROVIDER(index)                                                                  \
    extern "C"                                                                                  \
    NTSTATUS                                                                                    \
        MaPicoCreateProcess##index(                                                             \
            _In_ PPS_PICO_PROCESS_ATTRIBUTES ProcessAttributes,                                 \
            _In_opt_ PPS_PICO_CREATE_INFO CreateInfo,                                           \
            _Outptr_ PHANDLE ProcessHandle                                                      \
        )                                                                                       \
    {                                                                                           \
        return MapCreateProcess(index, ProcessAttributes, CreateInfo, ProcessHandle);           \
    }                                                                                           \
    extern "C"                                                                                  \
    NTSTATUS                                                                                    \
        MaPicoCreateProcessTh1##index(                                                          \
            _In_ PPS_PICO_PROCESS_ATTRIBUTES ProcessAttributes,                                 \
            _Outptr_ PHANDLE ProcessHandle                                                      \
        )                                                                                       \
    {                                                                                           \
        return MapCreateProcess(index, ProcessAttributes, NULL, ProcessHandle);                 \
    }                                                                                           \
                                                                                                \
    extern "C"                                                                                  \
    NTSTATUS                                                                                    \
        MaPicoCreateThread##index(                                                              \
            _In_ PPS_PICO_THREAD_ATTRIBUTES ThreadAttributes,                                   \
            _In_opt_ PPS_PICO_CREATE_INFO CreateInfo,                                           \
            _Outptr_ PHANDLE ThreadHandle                                                       \
        )                                                                                       \
    {                                                                                           \
        return MapCreateThread(index, ThreadAttributes, CreateInfo, ThreadHandle);              \
    }                                                                                           \
    extern "C"                                                                                  \
    NTSTATUS                                                                                    \
        MaPicoCreateThreadTh1##index(                                                           \
            _In_ PPS_PICO_THREAD_ATTRIBUTES ThreadAttributes,                                   \
            _Outptr_ PHANDLE ThreadHandle                                                       \
        )                                                                                       \
    {                                                                                           \
        return MapCreateThread(index, ThreadAttributes, NULL, ThreadHandle);                    \
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
