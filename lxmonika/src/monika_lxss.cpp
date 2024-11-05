#include "monika.h"

#include <ntifs.h>

#include "device.h"
#include "lxss.h"
#include "module.h"

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
// LXSS hook initialization
//

BOOLEAN MapLxssPatched = FALSE;
BOOLEAN MapLxssRegistering = FALSE;
SIZE_T MapLxssProviderIndex = (SIZE_T)-1;

//static PVOID MaLxssOriginalImportValue = NULL;
static CHAR MaLxssTrampolineBytes[MDL_TRAMPOLINE_SIZE];
static BOOLEAN MaLxssTrampolineInstalled = FALSE;
static PPS_PICO_PROVIDER_SYSTEM_CALL_DISPATCH MaLxssOriginalDispatchSystemCall = NULL;

extern "C"
NTSTATUS
MapLxssInitialize(
    _Inout_ PDRIVER_OBJECT DriverObject
)
{
    // Optional step: Register WSL as a lxmonika client to simplify Pico process routines handling.
    HANDLE hdlLxCore;
    MA_RETURN_IF_FAIL(MdlpFindModuleByName("lxcore.sys", &hdlLxCore, NULL));

    if (!MapTooLate)
    {
        // We are loaded early and successfully registered ourselves as a Pico provider.
        // We are free from PatchGuard's wrath, and can temporarily patch lxcore to use
        // our own Pico registration function.

        // The cleaner method of patching IATs for PsRegisterPicoProvider was tried.
        // The patching succeeded, but somehow lxcore had all its ntoskrnl.exe imports
        // magically baked in when loaded into memory and skipped the IAT altogether.

        //MA_RETURN_IF_FAIL(MdlpPatchImport(
        //    hdlLxCore,
        //    "ntoskrnl.exe",
        //    "PsRegisterPicoProvider",
        //    &MaLxssOriginalImportValue
        //));

        // Therefore we have to directly patch the function in ntoskrnl with a trampoline.
        // These trampolines are architecture-specific and requires special shellcode.
        //
        // Since we are doing this very early, before PatchGuard initialization, we should
        // not worry about having to patch the kernel. MapLxssPrepareForPatchGuard will
        // restore the original code before DriverEntry returns.

        MA_RETURN_IF_FAIL(MdlpPatchTrampoline(
            PsRegisterPicoProvider,
            MaRegisterPicoProvider,
            MaLxssTrampolineBytes,
            sizeof(MaLxssTrampolineBytes)
        ));

        MaLxssTrampolineInstalled = TRUE;

        LX_SUBSYSTEM lxSubsystem = { };

        MapLxssRegistering = TRUE;
        NTSTATUS statusLxInitialize = LxInitialize(DriverObject, &lxSubsystem);
        MapLxssRegistering = FALSE;
        MA_RETURN_IF_FAIL(statusLxInitialize);

        // It's likely that some new devices have been registered.
        // Handle them.
        // We do not need to unload those during cleanup, since MapLxssCleanup
        // does not delete any device object, so operations on them are still
        // very possible.
        MA_RETURN_IF_FAIL(DevpRegisterDeviceSet(DriverObject, NULL));

        if (MapLxssProviderIndex != (SIZE_T)-1)
        {
            // lxcore successfully registered itself as a Pico provider.

            // Hook the system call dispatching callback.
            MaLxssOriginalDispatchSystemCall =
                MapProviderRoutines[MapLxssProviderIndex].DispatchSystemCall;
            MapProviderRoutines[MapLxssProviderIndex].DispatchSystemCall = MapLxssSystemCallHook;

            // Our own extensions.
            ULONG ulAbiVersion = MapAdditionalProviderRoutines[MapLxssProviderIndex].AbiVersion;
            MapAdditionalProviderRoutines[MapLxssProviderIndex] =
            {
                .Size = sizeof(MA_PICO_PROVIDER_ROUTINES),
                .GetAllocatedProviderName = MapLxssGetAllocatedProviderName,
                .GetConsole = MapLxssGetConsole,
                .AbiVersion = ulAbiVersion
            };

            // To prevent lxcore from registering itself a second time in RlpInitializeDevices,
            // we disable Pico registration for now.
            MapPicoRegistrationDisabled = TRUE;
        }
        else
        {
            // The initialization routine succeeded, but no registration occurred.
            // This probably means we are relying on the lxstub instead of Microsoft's lxcore.
            // (Or there might have been some issues with our hook).
            Logger::LogWarning("Did not detect a Pico registration from lxcore.");

            return STATUS_UNSUCCESSFUL;
        }
    }
    else // if (MapTooLate) // We are too late
    {
        PPS_PICO_ROUTINES pLxpRoutines = NULL;

        // We are loaded late. lxcore has already been initialized by lxss.
        MA_RETURN_IF_FAIL(MdlpGetProcAddress(hdlLxCore, "LxpRoutines", (PVOID*)&pLxpRoutines));

        Logger::LogTrace("lxcore.sys detected. Found LxpRoutines at ", pLxpRoutines);

        // The "original" provider routines in this case are those registered by lxcore.
        PS_PICO_PROVIDER_ROUTINES lxProviderRoutines = MapOriginalProviderRoutines;

        // Hook the system call dispatching callback.
        MaLxssOriginalDispatchSystemCall = lxProviderRoutines.DispatchSystemCall;
        lxProviderRoutines.DispatchSystemCall = MapLxssSystemCallHook;

        // Our own extensions
        MA_PICO_PROVIDER_ROUTINES lxAdditionalProviderRoutines =
        {
            .Size = sizeof(MA_PICO_PROVIDER_ROUTINES),
            .GetAllocatedProviderName = MapLxssGetAllocatedProviderName,
            .GetConsole = MapLxssGetConsole
        };

        // Ignored but filled to prevent the function from complaining.
        MA_PICO_ROUTINES lxAdditionalRoutines
        {
            .Size = sizeof(MA_PICO_ROUTINES)
        };

        MA_RETURN_IF_FAIL(MaRegisterPicoProviderEx(
            &lxProviderRoutines,
            pLxpRoutines,
            &lxAdditionalProviderRoutines,
            &lxAdditionalRoutines,
            &MapLxssProviderIndex
        ));
    }

    Logger::LogTrace("lxcore.sys successfully registered as a lxmonika provider.");

    MapLxssPatched = TRUE;
    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
MapLxssPrepareForPatchGuard()
{

    if (!MapTooLate)
    {
        // Unpatch everything, preparing for PatchGuard initialization.

        //HANDLE hdlLxCore;
        //MA_RETURN_IF_FAIL(MdlpFindModuleByName("lxcore.sys", &hdlLxCore, NULL));

        //MA_RETURN_IF_FAIL(MdlpPatchImport(
        //    hdlLxCore,
        //    "ntoskrnl.exe",
        //    "PsRegisterPicoProvider",
        //    &MaLxssOriginalImportValue
        //));

        if (MaLxssTrampolineInstalled)
        {
            MA_RETURN_IF_FAIL(MdlpPatchTrampoline(
                PsRegisterPicoProvider,
                NULL,
                MaLxssTrampolineBytes,
                sizeof(MaLxssTrampolineBytes)
            ));

            MaLxssTrampolineInstalled = FALSE;
        }

        // lxcore should have finished initializing. Enable Pico registration again.
        MapPicoRegistrationDisabled = FALSE;
    }
    else
    {
        // Nothing to do here, PatchGuard has started way before we load.
    }

    return STATUS_SUCCESS;
}

extern "C"
VOID
MapLxssCleanup()
{
    if (!MapLxssPatched)
    {
        return;
    }

    PPS_PICO_ROUTINES pLxpRoutines = NULL;
    HANDLE hdlLxCore;
    if (NT_SUCCESS(MdlpFindModuleByName("lxcore.sys", &hdlLxCore, NULL))
        && NT_SUCCESS(MdlpGetProcAddress(hdlLxCore, "LxpRoutines",
            (PVOID*)&pLxpRoutines)))
    {
        // Whether we are too late or not, MapOriginalRoutines contains whatever the system would
        // bless its one and only official Pico provider with. We pass that back to lxcore.
        *pLxpRoutines = MapOriginalRoutines;
    }

    MapLxssPatched = FALSE;
}


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
#elif defined(_M_IX86)
    if (pSyscallInfo->TrapFrame->Eax == 122)
    {
        pUtsName = (old_utsname*)pSyscallInfo->TrapFrame->Ebx;
    }
#elif defined(_M_ARM)
    if (pSyscallInfo->R7 == 122)
    {
        pUtsName = (old_utsname*)pSyscallInfo->TrapFrame->R0;
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

    MaLxssOriginalDispatchSystemCall(pSyscallInfo);

    if (pUtsName != NULL
        // Also check for a success return value.
        // Otherwise, pUtsName may be an invalid pointer.
#ifdef _M_X64
        && pSyscallInfo->TrapFrame->Rax == 0)
#elif defined(_M_ARM64)
        && pSyscallInfo->TrapFrame->X0 == 0)
#elif defined(_M_IX86)
        && pSyscallInfo->TrapFrame->Eax == 0)
#elif defined(_M_ARM)
        && pSyscallInfo->TrapFrame->R0 == 0)
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
