#include "monika.h"

#include <ntddk.h>

#include "Logger.h"

//
// Monika macros
//

#ifndef MONIKA_KERNEL_TIMESTAMP
#ifdef MONIKA_TIMESTAMP
#define MONIKA_KERNEL_TIMESTAMP     MONIKA_TIMESTAMP
#else
#define MONIKA_KERNEL_TIMESTAMP     "Fri Jan 01 08:00:00 PST 2016"
#endif
#endif

#ifndef MONIKA_KERNEL_VERSION
#define MONIKA_KERNEL_VERSION       "6.6.0"
#endif

#ifndef MONIKA_KERNEL_BUILD_NUMBER
#ifdef MONIKA_BUILD_NUMBER
#define MONIKA_KERNEL_BUILD_NUMBER  MONIKA_BUILD_NUMBER
#else
#define MONIKA_KERNEL_BUILD_NUMBER  "1"
#endif
#endif

//
// Linux syscall hooks
//

extern "C"
VOID
MapLxssSystemCallHook(
    _In_ PPS_PICO_SYSTEM_CALL_INFORMATION pSyscallInfo
)
{
    BOOLEAN bIsUname = FALSE;

#ifdef AMD64
    // Check for SYS_uname
    bIsUname = pSyscallInfo->TrapFrame->Rax == 63;
    if (bIsUname)
    {
        Logger::LogTrace("uname(", (PVOID)pSyscallInfo->TrapFrame->Rdi, ")");
    }
#else
#error Detect the syscall arguments for this architecture!
#endif

    MapOriginalProviderRoutines.DispatchSystemCall(pSyscallInfo);

    if (bIsUname
        // Also check for a success return value.
        // Otherwise, pUtsName may be an invalid pointer.
#ifdef AMD64
        && pSyscallInfo->TrapFrame->Rax == 0)
#else
#error Detect the syscall return value for this architecture!
#endif
    {
        struct old_utsname {
            char sysname[65];
            char nodename[65];
            char release[65];
            char version[65];
            char machine[65];
        }* pUtsName = NULL;

        // We should be in the context of the calling process.
        // Therefore, it is safe to access the raw pointers.
        pUtsName = (old_utsname*)pSyscallInfo->TrapFrame->Rdi;

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
    }
}
