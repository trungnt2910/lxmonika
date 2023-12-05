#include "monika.h"

#include <ntddk.h>

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
// Linux syscall hooks
//

BOOLEAN
MapPreSyscallHook(
    _In_ PPS_PICO_SYSTEM_CALL_INFORMATION pSyscallInfo
)
{
    UNREFERENCED_PARAMETER(pSyscallInfo);

    // Return TRUE to allow the Linux syscall to proceed.
    return TRUE;
}

VOID
MapPostSyscallHook(
    _In_ PPS_PICO_SYSTEM_CALL_INFORMATION pPreviousSyscallInfo,
    _In_ PPS_PICO_SYSTEM_CALL_INFORMATION pCurrentSyscallInfo
)
{
    BOOLEAN bIsUname = FALSE;

#ifdef AMD64
    // Check for SYS_uname
    bIsUname = pPreviousSyscallInfo->TrapFrame->Rax == 63;
    if (bIsUname)
    {
        Logger::LogTrace("uname(", (PVOID)pPreviousSyscallInfo->TrapFrame->Rdi, ")");
    }
#else
#error Detect the syscall arguments for this architecture!
#endif

    if (bIsUname
        // Also check for a success return value.
        // Otherwise, pUtsName may be an invalid pointer.
#ifdef AMD64
        && pCurrentSyscallInfo->TrapFrame->Rax == 0)
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
        pUtsName = (old_utsname*)pCurrentSyscallInfo->TrapFrame->Rdi;

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
