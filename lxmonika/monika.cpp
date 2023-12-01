#include "monika.h"

#include "picosupport.h"

#include "Logger.h"

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
// Monika data
//

static PS_PICO_PROVIDER_ROUTINES MaOriginalProviderRoutines;
static PS_PICO_ROUTINES MaOriginalRoutines;

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

    // All known versions of the struct contains valid pointers
    // for these members, so this should be safe.
    pProviderRoutines->DispatchSystemCall = MapSystemCallDispatch;
    pProviderRoutines->ExitThread = MapThreadExit;
    pProviderRoutines->ExitProcess = MapProcessExit;
    pProviderRoutines->DispatchException = MapDispatchException;
    pProviderRoutines->TerminateProcess = MapTerminateProcess;
    pProviderRoutines->WalkUserStack = MapWalkUserStack;

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
}

//
// Pico handlers
//
extern "C"
VOID
MapSystemCallDispatch(
    _In_ PPS_PICO_SYSTEM_CALL_INFORMATION SystemCall
)
{
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
    MaOriginalProviderRoutines.ExitThread(Thread);
}

extern "C"
VOID
MapProcessExit(
    _In_ PEPROCESS Process
)
{
    MaOriginalProviderRoutines.ExitProcess(Process);
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
    return MaOriginalProviderRoutines.WalkUserStack(
        TrapFrame,
        Callers,
        FrameCount
    );
}
