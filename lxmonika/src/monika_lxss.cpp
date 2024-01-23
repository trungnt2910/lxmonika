#include "monika.h"

#include <ntifs.h>

#include "lxss.h"

#include "AutoResource.h"
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
// LXSS hooked provider routines
//

extern "C"
VOID
MapLxssSystemCallHook(
    _In_ PPS_PICO_SYSTEM_CALL_INFORMATION pSyscallInfo
)
{
    struct old_utsname {
        char sysname[65];
        char nodename[65];
        char release[65];
        char version[65];
        char machine[65];
    }* pUtsName = NULL;

    // Check for SYS_uname
#ifdef _M_X64
    if (pSyscallInfo->TrapFrame->Rax == 63)
    {
        pUtsName = (old_utsname*)pSyscallInfo->TrapFrame->Rdi;
    }
#elif defined(_M_ARM64)
    if (pSyscallInfo->TrapFrame->X8 == 160)
    {
        pUtsName = (old_utsname*)pSyscallInfo->TrapFrame->X0;
    }
#else
#error Detect the syscall arguments for this architecture!
#endif

    // Compared to the old checks, this will miss `uname` calls where the argument is NULL.
    // However, we do not intend to intercept such calls anyway.
    if (pUtsName != NULL)
    {
        Logger::LogTrace("uname(", pUtsName, ")");
    }

    MapOriginalProviderRoutines.DispatchSystemCall(pSyscallInfo);

    if (pUtsName != NULL
        // Also check for a success return value.
        // Otherwise, pUtsName may be an invalid pointer.
#ifdef _M_X64
        && pSyscallInfo->TrapFrame->Rax == 0)
#elif defined(_M_ARM64)
        && pSyscallInfo->TrapFrame->X0 == 0)
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
    }
}

// The release string has never been changed since Windows 10,
// so it is safe to hard code it here.
// We do not really need the Windows NT build number, otherwise a dynamic query to
// fetch version string resources would be required.
static UNICODE_STRING MaLxssProviderName = RTL_CONSTANT_STRING(L"Linux-4.4.0-Microsoft-WSL1");

NTSTATUS
MapLxssGetAllocatedProviderName(
    _Outptr_ PUNICODE_STRING* pOutProviderName
)
{
    *pOutProviderName = &MaLxssProviderName;
    return STATUS_SUCCESS;
}

NTSTATUS
MapLxssGetConsole(
    _In_ PEPROCESS Process,
    _Out_opt_ PHANDLE Console,
    _Out_opt_ PHANDLE Input,
    _Out_opt_ PHANDLE Output
)
{
    HANDLE hdlConsole = NULL;
    AUTO_RESOURCE(hdlConsole, ZwClose);
    HANDLE hdlInput = NULL;
    AUTO_RESOURCE(hdlInput, ZwClose);
    HANDLE hdlOutput = NULL;
    AUTO_RESOURCE(hdlOutput, ZwClose);

    __try
    {
        PLX_PROCESS pLxProcess = (PLX_PROCESS)
            MaPicoGetProcessContext0(Process);

        PLX_SESSION pLxSession = CONTAINING_RECORD(
            pLxProcess->ThreadGroups.Flink, LX_THREAD_GROUP, ListEntry)
                ->ProcessGroup->Session;
        PLX_VFS_DEV_TTY_DEVICE pSessionTerminal =
            (PLX_VFS_DEV_TTY_DEVICE)pLxSession->SessionTerminal;

        if (pSessionTerminal == NULL)
        {
            return STATUS_NOT_FOUND;
        }

        if (Console != NULL)
        {
            MA_RETURN_IF_FAIL(MaUtilDuplicateKernelHandle(
                pSessionTerminal->ConsoleState->Console,
                &hdlConsole
            ));
        }

        if (Input != NULL)
        {
            MA_RETURN_IF_FAIL(MaUtilDuplicateKernelHandle(
                pSessionTerminal->ConsoleState->Input,
                &hdlInput
            ));
        }

        if (Output != NULL)
        {
            MA_RETURN_IF_FAIL(MaUtilDuplicateKernelHandle(
                pSessionTerminal->ConsoleState->Output,
                &hdlOutput
            ));
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Console != NULL)
    {
        *Console = hdlConsole;
        hdlConsole = NULL;
    }

    if (Input != NULL)
    {
        *Input = hdlInput;
        hdlInput = NULL;
    }

    if (Output != NULL)
    {
        *Output = hdlOutput;
        hdlOutput = NULL;
    }

    return STATUS_SUCCESS;
}
