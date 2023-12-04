#include "monika.h"

#include "os.h"
#include "picosupport.h"

#include "Logger.h"
#include "ProcessMap.h"

//
// Monika macros
//

#ifndef MONIKA_KERNEL_TIMESTAMP
#define MONIKA_KERNEL_TIMESTAMP     "Fri Jan 01 08:00:00 PST 2016"
#endif

#ifndef MONIKA_KERNEL_VERSION
#define MONIKA_KERNEL_VERSION       "6.6.0"
#endif

#ifndef MONIKA_KERNEL_BUILD_NUMBER
#define MONIKA_KERNEL_BUILD_NUMBER  "1"
#endif

//
// Monika definitions
//

enum
{
#define MONIKA_PROVIDER(index) MaPicoProvider##index = index,
#include "monika_providers.cpp"
#undef MONIKA_PROVIDER
    MaPicoProviderMaxCount
};

#define MONIKA_PROVIDER(index)                                                          \
    static PS_PICO_CREATE_PROCESS               MaPicoCreateProcess##index;             \
    static PS_PICO_CREATE_THREAD                MaPicoCreateThread##index;              \
    static PS_PICO_GET_PROCESS_CONTEXT          MaPicoGetProcessContext##index;         \
    static PS_PICO_GET_THREAD_CONTEXT           MaPicoGetThreadContext##index;          \
    static PS_PICO_SET_THREAD_DESCRIPTOR_BASE   MaPicoSetThreadDescriptorBase##index;   \
    static PS_PICO_TERMINATE_PROCESS            MaPicoTerminateProcess##index;          \
    static PS_SET_CONTEXT_THREAD_INTERNAL       MaPicoSetContextThreadInternal##index;  \
    static PS_GET_CONTEXT_THREAD_INTERNAL       MaPicoGetContextThreadInternal##index;  \
    static PS_TERMINATE_THREAD                  MaPicoTerminateThread##index;           \
    static PS_SUSPEND_THREAD                    MaPicoSuspendThread##index;             \
    static PS_RESUME_THREAD                     MaPicoResumeThread##index;
#include "monika_providers.cpp"
#undef MONIKA_PROVIDER

//
// Monika data
//

static PS_PICO_PROVIDER_ROUTINES MaOriginalProviderRoutines;
static PS_PICO_ROUTINES MaOriginalRoutines;

static SIZE_T MaProvidersCount = 0;
static PS_PICO_PROVIDER_ROUTINES MaProviderRoutines[MaPicoProviderMaxCount];
static PS_PICO_ROUTINES MaRoutines[MaPicoProviderMaxCount];

static ProcessMap MaProcessMap;

//
// Monika lifetime functions
//

extern "C"
NTSTATUS
MapInitialize()
{
    NTSTATUS status;

    PPS_PICO_ROUTINES pRoutines = NULL;
    status = PicoSppLocateRoutines(&pRoutines);

    Logger::LogTrace("PicoSppLocateRoutines status=", (void*)status);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    memcpy(&MaOriginalRoutines, pRoutines, sizeof(MaOriginalRoutines));

    if (pRoutines->Size != sizeof(MaOriginalRoutines))
    {
        Logger::LogWarning("Expected size ", sizeof(MaOriginalRoutines),
            " for struct PS_PICO_ROUTINES, but got ", pRoutines->Size);
        Logger::LogWarning("Please update the pico struct definitions.");
    }

    PPS_PICO_PROVIDER_ROUTINES pProviderRoutines = NULL;
    status = PicoSppLocateProviderRoutines(&pProviderRoutines);

    Logger::LogTrace("PicoSppLocateProviderRoutines status=", (void*)status);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    memcpy(&MaOriginalProviderRoutines, pProviderRoutines, sizeof(MaOriginalProviderRoutines));

    Logger::LogTrace("Backed up original provider routines.");

    // All known versions of the struct contains valid pointers
    // for these members, so this should be safe.
    pProviderRoutines->DispatchSystemCall = MapSystemCallDispatch;
    pProviderRoutines->ExitThread = MapThreadExit;
    pProviderRoutines->ExitProcess = MapProcessExit;
    pProviderRoutines->DispatchException = MapDispatchException;
    pProviderRoutines->TerminateProcess = MapTerminateProcess;
    pProviderRoutines->WalkUserStack = MapWalkUserStack;

    Logger::LogTrace("Successfully patched provider routines.");

    // Initialize pico routines
#define MONIKA_PROVIDER(index)                                              \
    MaRoutines[MaPicoProvider##index] =                                     \
    {                                                                       \
        .Size = sizeof(PS_PICO_ROUTINES),                                   \
        .CreateProcess = MaPicoCreateProcess##index,                        \
        .CreateThread = MaPicoCreateThread##index,                          \
        .GetProcessContext = MaPicoGetProcessContext##index,                \
        .GetThreadContext = MaPicoGetThreadContext##index,                  \
        .GetContextThreadInternal = MaPicoGetContextThreadInternal##index,  \
        .SetContextThreadInternal = MaPicoSetContextThreadInternal##index,  \
        .TerminateThread = MaPicoTerminateThread##index,                    \
        .ResumeThread = MaPicoResumeThread##index,                          \
        .SetThreadDescriptorBase = MaPicoSetThreadDescriptorBase##index,    \
        .SuspendThread = MaPicoSuspendThread##index,                        \
        .TerminateProcess = MaPicoTerminateProcess##index                   \
    };
#include "monika_providers.cpp"
#undef MONIKA_PROVIDER

    // Initialize process map
    MaProcessMap.Initialize();

    return STATUS_SUCCESS;
}

extern "C"
VOID
MapCleanup()
{
    PPS_PICO_PROVIDER_ROUTINES pRoutines = NULL;
    if (NT_SUCCESS(PicoSppLocateProviderRoutines(&pRoutines)))
    {
        memcpy(pRoutines, &MaOriginalProviderRoutines, sizeof(MaOriginalProviderRoutines));
    }
    MaProcessMap.Clear();
}


//
// Pico provider registration
//

extern "C"
__declspec(dllexport)
NTSTATUS NTAPI
MaRegisterPicoProvider(
    _In_ PPS_PICO_PROVIDER_ROUTINES ProviderRoutines,
    _Inout_ PPS_PICO_ROUTINES PicoRoutines
)
{
    if (ProviderRoutines->Size != sizeof(PS_PICO_PROVIDER_ROUTINES)
        || PicoRoutines->Size != sizeof(PS_PICO_ROUTINES))
    {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    const ACCESS_MASK uPicoMaximumRights = STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL;
    if ((ProviderRoutines->OpenProcess & ~uPicoMaximumRights) != 0
        || (ProviderRoutines->OpenThread & ~uPicoMaximumRights) != 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Acquire an index for the provider.
    // Once a provider has been registered, it cannot be unregistered.
    // This is the same as the NT kernel as PsUnregisterPicoProvider does not exist.
    SIZE_T uProviderIndex = InterlockedIncrementSizeT(&MaProvidersCount) - 1;
    if (uProviderIndex >= MaPicoProviderMaxCount)
    {
        // Prevent SIZE_T overflow (quite hard on 64-bit systems).
        InterlockedDecrementSizeT(&MaProvidersCount);

        // PsRegisterPicoProvider would return STATUS_TOO_LATE here.
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    MaProviderRoutines[uProviderIndex] = *ProviderRoutines;
    MaRoutines[uProviderIndex] = *PicoRoutines;

    return STATUS_SUCCESS;
}

//
// Pico provider dispatchers
//

#define MA_LOCK_PROCESS_MAP auto _ = Locker(&MaProcessMap)

#define MA_TRY_DISPATCH_TO_HANDLER(process, function, ...)                                      \
    do                                                                                          \
    {                                                                                           \
                                                                                                \
        BOOLEAN bHasHandler = FALSE;                                                            \
        DWORD dwHandler;                                                                        \
                                                                                                \
        {                                                                                       \
            MA_LOCK_PROCESS_MAP;                                                                \
            if (NT_SUCCESS(MaProcessMap.GetProcessHandler((process), &dwHandler)))              \
            {                                                                                   \
                bHasHandler = TRUE;                                                             \
            }                                                                                   \
        }                                                                                       \
                                                                                                \
        if (bHasHandler)                                                                        \
        {                                                                                       \
            return MaProviderRoutines[dwHandler].function(__VA_ARGS__);                         \
        }                                                                                       \
    } while (false)

extern "C"
VOID
MapSystemCallDispatch(
    _In_ PPS_PICO_SYSTEM_CALL_INFORMATION SystemCall
)
{
    MA_TRY_DISPATCH_TO_HANDLER(PsGetCurrentProcess(), DispatchSystemCall, SystemCall);

    bool bIsUname = false;
    struct old_utsname {
        char sysname[65];
        char nodename[65];
        char release[65];
        char version[65];
        char machine[65];
    }* pUtsName = NULL;

#ifdef AMD64
    // Check for SYS_uname
    bIsUname = SystemCall->TrapFrame->Rax == 63;
    if (bIsUname)
    {
        pUtsName = (old_utsname*)SystemCall->TrapFrame->Rdi;
        Logger::LogTrace("uname(", (PVOID)SystemCall->TrapFrame->Rdi, ")");
    }
#else
#error Detect the syscall arguments for this architecture!
#endif

    MaOriginalProviderRoutines.DispatchSystemCall(SystemCall);

    if (bIsUname
        // Also check for a success return value.
        // Otherwise, pUtsName may be an invalid pointer.
#ifdef AMD64
        && SystemCall->TrapFrame->Rax == 0)
#else
#error Detect the syscall return value for this architecture!
#endif
    {
        // We should be in the context of the calling process.
        // Therefore, it is safe to access the raw pointers.

        Logger::LogTrace("pUtsName->sysname  ", (PCSTR)pUtsName->sysname);
        Logger::LogTrace("pUtsName->nodename ", (PCSTR)pUtsName->nodename);
        Logger::LogTrace("pUtsName->release  ", (PCSTR)pUtsName->release);
        Logger::LogTrace("pUtsName->version  ", (PCSTR)pUtsName->version);
        Logger::LogTrace("pUtsName->machine  ", (PCSTR)pUtsName->machine);

        strcpy(pUtsName->sysname, "Monika");
        // "Microsoft" (case sensitive) is required.
        // Otherwise, Microsoft's `init` will detect the kernel as WSL2.
        strncpy(pUtsName->release, MONIKA_KERNEL_VERSION "-Monika-Microsoft",
            sizeof(pUtsName->release));
        strncpy(pUtsName->version,
            "#" MONIKA_KERNEL_BUILD_NUMBER "-Just-Monika " MONIKA_KERNEL_TIMESTAMP,
            sizeof(pUtsName->version));

        Logger::LogTrace("New pUtsName->release  ", (PCSTR)pUtsName->release);
    }
}

extern "C"
VOID
MapThreadExit(
    _In_ PETHREAD Thread
)
{
    MA_TRY_DISPATCH_TO_HANDLER(PsGetThreadProcess(Thread), ExitThread, Thread);

    return MaOriginalProviderRoutines.ExitThread(Thread);
}

extern "C"
VOID
MapProcessExit(
    _In_ PEPROCESS Process
)
{
    BOOLEAN bHasHandler = FALSE;
    PROCESS_HANDLER_INFORMATION info;

    {
        MA_LOCK_PROCESS_MAP;
        if (NT_SUCCESS(MaProcessMap.GetProcessHandler(Process, &info)))
        {
            bHasHandler = TRUE;
            MaProcessMap.UnregisterProcess(Process);
        }
    }

    if (bHasHandler)
    {
        MaProviderRoutines[info.Handler].ExitProcess(Process);
    }

    if (bHasHandler && info.HasInternalParentHandler)
    {
        MaProviderRoutines[info.ParentHandler].ExitProcess(Process);
    }

    if (!bHasHandler || !info.HasInternalParentHandler)
    {
        MaOriginalProviderRoutines.ExitProcess(Process);
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
    MA_TRY_DISPATCH_TO_HANDLER(PsGetCurrentProcess(), DispatchException,
        ExceptionRecord,
        ExceptionFrame,
        TrapFrame,
        Chance,
        PreviousMode
    );

    return MaOriginalProviderRoutines.DispatchException(
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
    MA_TRY_DISPATCH_TO_HANDLER(Process, TerminateProcess, Process, TerminateStatus);

    return MaOriginalProviderRoutines.TerminateProcess(
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
    MA_TRY_DISPATCH_TO_HANDLER(PsGetCurrentProcess(), WalkUserStack,
        TrapFrame,
        Callers,
        FrameCount
    );

    return MaOriginalProviderRoutines.WalkUserStack(
        TrapFrame,
        Callers,
        FrameCount
    );
}

#define MA_PROCESS_BELONGS_TO_HANDLER(process, index)                                           \
    (([](PEPROCESS p, DWORD i)                                                                  \
    {                                                                                           \
        MA_LOCK_PROCESS_MAP;                                                                    \
        return MaProcessMap.ProcessBelongsToHandler(p, i);                                      \
    })((process), (index)))

static
NTSTATUS
MapCreateProcess(
    _In_ DWORD HandlerIndex,
    _In_ PPS_PICO_PROCESS_ATTRIBUTES ProcessAttributes,
    _Outptr_ PHANDLE ProcessHandle
)
{
    NTSTATUS status = MaOriginalRoutines.CreateProcess(ProcessAttributes, ProcessHandle);

    // TODO: Check if this "HANDLE" is actually the same as a `PEPROCESS` struct?
    if (NT_SUCCESS(status))
    {
        {
            MA_LOCK_PROCESS_MAP;
            status = MaProcessMap.RegisterProcessHandler((PEPROCESS)*ProcessHandle, HandlerIndex);
        }

        if (!NT_SUCCESS(status))
        {
            MaOriginalRoutines.TerminateProcess((PEPROCESS)*ProcessHandle, status);
        }
    }

    return status;
}

static
NTSTATUS
MapCreateThread(
    _In_ DWORD HandlerIndex,
    _In_ PPS_PICO_THREAD_ATTRIBUTES ThreadAttributes,
    _Outptr_ PHANDLE ThreadHandle
)
{
    if (ThreadAttributes == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (!MA_PROCESS_BELONGS_TO_HANDLER((PEPROCESS)ThreadAttributes->Process, HandlerIndex))
    {
        return STATUS_INVALID_PARAMETER;
    }

    return MaOriginalRoutines.CreateThread(ThreadAttributes, ThreadHandle);
}

static
PVOID
MapGetProcessContext(
    _In_ DWORD HandlerIndex,
    _In_ PEPROCESS Process
)
{
    if (!MA_PROCESS_BELONGS_TO_HANDLER(Process, HandlerIndex))
    {
        // TODO: Return what on error?
        return NULL;
    }

    return MaOriginalRoutines.GetProcessContext(Process);
}

static
PVOID
MapGetThreadContext(
    _In_ DWORD HandlerIndex,
    _In_ PETHREAD Thread
)
{
    if (!MA_PROCESS_BELONGS_TO_HANDLER(PsGetThreadProcess(Thread), HandlerIndex))
    {
        // TODO: Return what on error?
        return NULL;
    }

    return MaOriginalRoutines.GetThreadContext(Thread);
}

static
VOID
MapSetThreadDescriptorBase(
    _In_ DWORD HandlerIndex,
    _In_ PS_PICO_THREAD_DESCRIPTOR_TYPE Type,
    _In_ ULONG_PTR Base
)
{
#if !DBG
    UNREFERENCED_PARAMETER(HandlerIndex);
#endif
    ASSERT(MA_PROCESS_BELONGS_TO_HANDLER(PsGetCurrentProcess(), HandlerIndex));

    return MaOriginalRoutines.SetThreadDescriptorBase(Type, Base);
}

static
NTSTATUS
MapTerminateProcess(
    _In_ DWORD HandlerIndex,
    _Inout_ PEPROCESS Process,
    _In_ NTSTATUS ExitStatus
)
{
    if (!MA_PROCESS_BELONGS_TO_HANDLER(Process, HandlerIndex))
    {
        return STATUS_INVALID_PARAMETER;
    }

    // TODO: Why __inout for the Process parameter?
    return MaOriginalRoutines.TerminateProcess(Process, ExitStatus);
}

static
NTSTATUS
MapSetContextThreadInternal(
    _In_ DWORD HandlerIndex,
    _In_ PETHREAD Thread,
    _In_ PCONTEXT ThreadContext,
    _In_ KPROCESSOR_MODE ProbeMode,
    _In_ KPROCESSOR_MODE CtxMode,
    _In_ BOOLEAN PerformUnwind
)
{
    if (!MA_PROCESS_BELONGS_TO_HANDLER(PsGetThreadProcess(Thread), HandlerIndex))
    {
        return STATUS_INVALID_PARAMETER;
    }

    return MaOriginalRoutines.SetContextThreadInternal(
        Thread, ThreadContext, ProbeMode, CtxMode, PerformUnwind);
}

static
NTSTATUS
MapGetContextThreadInternal(
    _In_ DWORD HandlerIndex,
    _In_ PETHREAD Thread,
    _Inout_ PCONTEXT ThreadContext,
    _In_ KPROCESSOR_MODE ProbeMode,
    _In_ KPROCESSOR_MODE CtxMode,
    _In_ BOOLEAN PerformUnwind
)
{
    if (!MA_PROCESS_BELONGS_TO_HANDLER(PsGetThreadProcess(Thread), HandlerIndex))
    {
        return STATUS_INVALID_PARAMETER;
    }

    return MaOriginalRoutines.GetContextThreadInternal(
        Thread, ThreadContext, ProbeMode, CtxMode, PerformUnwind);
}

static
NTSTATUS
MapTerminateThread(
    _In_ DWORD HandlerIndex,
    _Inout_ PETHREAD Thread,
    _In_ NTSTATUS ExitStatus,
    _In_ BOOLEAN DirectTerminate
)
{
    if (!MA_PROCESS_BELONGS_TO_HANDLER(PsGetThreadProcess(Thread), HandlerIndex))
    {
        return STATUS_INVALID_PARAMETER;
    }

    return MaOriginalRoutines.TerminateThread(Thread, ExitStatus, DirectTerminate);
}

static
NTSTATUS
MapSuspendThread(
    _In_ DWORD HandlerIndex,
    _Inout_ PETHREAD Thread,
    _Out_opt_ PULONG PreviousSuspendCount
)
{
    if (!MA_PROCESS_BELONGS_TO_HANDLER(PsGetThreadProcess(Thread), HandlerIndex))
    {
        return STATUS_INVALID_PARAMETER;
    }

    return MaOriginalRoutines.SuspendThread(Thread, PreviousSuspendCount);
}

static
NTSTATUS
MapResumeThread(
    _In_ DWORD HandlerIndex,
    _Inout_ PETHREAD Thread,
    _Out_opt_ PULONG PreviousSuspendCount
)
{
    if (!MA_PROCESS_BELONGS_TO_HANDLER(PsGetThreadProcess(Thread), HandlerIndex))
    {
        return STATUS_INVALID_PARAMETER;
    }

    return MaOriginalRoutines.ResumeThread(Thread, PreviousSuspendCount);
}

#define MONIKA_PROVIDER(index)                                                                  \
    static NTSTATUS                                                                             \
        MaPicoCreateProcess##index(                                                             \
            _In_ PPS_PICO_PROCESS_ATTRIBUTES ProcessAttributes,                                 \
            _Outptr_ PHANDLE ProcessHandle                                                      \
        )                                                                                       \
    {                                                                                           \
        return MapCreateProcess(index, ProcessAttributes, ProcessHandle);                       \
    }                                                                                           \
                                                                                                \
    static NTSTATUS                                                                             \
        MaPicoCreateThread##index(                                                              \
            _In_ PPS_PICO_THREAD_ATTRIBUTES ThreadAttributes,                                   \
            _Outptr_ PHANDLE ThreadHandle                                                       \
        )                                                                                       \
    {                                                                                           \
        return MapCreateThread(index, ThreadAttributes, ThreadHandle);                          \
    }                                                                                           \
                                                                                                \
    static PVOID                                                                                \
        MaPicoGetProcessContext##index(                                                         \
            _In_ PEPROCESS Process                                                              \
        )                                                                                       \
    {                                                                                           \
        return MapGetProcessContext(index, Process);                                            \
    }                                                                                           \
                                                                                                \
    static PVOID                                                                                \
        MaPicoGetThreadContext##index(                                                          \
            _In_ PETHREAD Thread                                                                \
        )                                                                                       \
    {                                                                                           \
        return MapGetThreadContext(index, Thread);                                              \
    }                                                                                           \
                                                                                                \
    static VOID                                                                                 \
        MaPicoSetThreadDescriptorBase##index(                                                   \
            _In_ PS_PICO_THREAD_DESCRIPTOR_TYPE Type,                                           \
            _In_ ULONG_PTR Base                                                                 \
        )                                                                                       \
    {                                                                                           \
        return MapSetThreadDescriptorBase(index, Type, Base);                                   \
    }                                                                                           \
                                                                                                \
    static NTSTATUS                                                                             \
        MaPicoTerminateProcess##index(                                                          \
            __inout PEPROCESS Process,                                                          \
            __in NTSTATUS ExitStatus                                                            \
        )                                                                                       \
    {                                                                                           \
        return MapTerminateProcess(index, Process, ExitStatus);                                 \
    }                                                                                           \
                                                                                                \
    static NTSTATUS                                                                             \
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
    static NTSTATUS                                                                             \
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
    static NTSTATUS                                                                             \
        MaPicoTerminateThread##index(                                                           \
            __inout PETHREAD Thread,                                                            \
            __in NTSTATUS ExitStatus,                                                           \
            __in BOOLEAN DirectTerminate                                                        \
        )                                                                                       \
    {                                                                                           \
        return MapTerminateThread(index, Thread, ExitStatus, DirectTerminate);                  \
    }                                                                                           \
                                                                                                \
    static NTSTATUS                                                                             \
        MaPicoSuspendThread##index(                                                             \
            _In_ PETHREAD Thread,                                                               \
            _Out_opt_ PULONG PreviousSuspendCount                                               \
        )                                                                                       \
    {                                                                                           \
        return MapSuspendThread(index, Thread, PreviousSuspendCount);                           \
    }                                                                                           \
                                                                                                \
    static NTSTATUS                                                                             \
        MaPicoResumeThread##index(                                                              \
            _In_ PETHREAD Thread,                                                               \
            _Out_opt_ PULONG PreviousSuspendCount                                               \
        )                                                                                       \
    {                                                                                           \
        return MapResumeThread(index, Thread, PreviousSuspendCount);                            \
    }
#include "monika_providers.cpp"
#undef MONIKA_PROVIDER
