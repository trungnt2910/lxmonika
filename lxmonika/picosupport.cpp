#include "picosupport.h"

#include <ntddk.h>
#include <wdm.h>

#include "module.h"

#include "Logger.h"

static PPS_PICO_PROVIDER_ROUTINES PspPicoProviderRoutines = NULL;

extern "C"
NTSTATUS
PicoSppLocateProviderRoutines(
    _Out_ PPS_PICO_PROVIDER_ROUTINES* pPpr
)
{
    NTSTATUS status = STATUS_SUCCESS;

    if (pPpr == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Method 0: Cached value
    if (PspPicoProviderRoutines != NULL)
    {
        *pPpr = PspPicoProviderRoutines;
        return STATUS_SUCCESS;
    }

    // Method 1: Known offsets from the NT Kernel base.
    // TODO: Method 1+: Fetch and read from symbol files.
    HANDLE hdlNtKernel = NULL;
    SIZE_T uNtKernelSize = 0;
    status = MdlpFindModuleByName("ntoskrnl.exe", &hdlNtKernel, &uNtKernelSize);

    Logger::LogTrace("Find ntoskrnl: status=", (void*)status, " handle=", hdlNtKernel,
        " size=", uNtKernelSize);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    RTL_OSVERSIONINFOW osVersionInfo;
    osVersionInfo.dwOSVersionInfoSize = sizeof(osVersionInfo);

    status = RtlGetVersion(&osVersionInfo);

    if (NT_SUCCESS(status))
    {
        Logger::LogTrace("Detected Windows NT build ", osVersionInfo.dwBuildNumber);

        PVOID pTarget = NULL;

        switch (osVersionInfo.dwBuildNumber)
        {
#ifdef AMD64
            // Windows 11 23H2
            case 22621:
                pTarget = (PCHAR)hdlNtKernel + 0xC37D00;
            break;
#endif
            // TODO: Support more builds.
            default:
                Logger::LogWarning("Windows NT build ",
                    osVersionInfo.dwBuildNumber, " is not supported.");
        }

        if (pTarget != NULL)
        {
            Logger::LogTrace("PspPicoProviderRoutines found at ", pTarget);
            *pPpr = PspPicoProviderRoutines = (PPS_PICO_PROVIDER_ROUTINES)pTarget;
            return STATUS_SUCCESS;
        }
    }

    // Method 2: Hunt for the structure in the .data section of ntoskrnl.
    PVOID pNtKernelData = NULL;
    SIZE_T uNtKernelDataSize = uNtKernelSize;
    status = MdlpFindModuleSectionByName(hdlNtKernel, ".data",
        &pNtKernelData, &uNtKernelDataSize);

    Logger::LogTrace("Find ntoskrnl data: status=", (void*)status, " data=", pNtKernelData,
        " size=", uNtKernelDataSize);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    HANDLE hdlLxCore = NULL;
    SIZE_T uLxCoreSize = 0;
    status = MdlpFindModuleByName("lxcore.sys", &hdlLxCore, &uLxCoreSize);

    Logger::LogTrace("Find lxcore: status=", (void*)status, " handle=", hdlLxCore,
        " size=", uLxCoreSize);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    PCHAR pSearchStart = (PCHAR)ALIGN_UP_POINTER_BY(pNtKernelData,
        alignof(PS_PICO_PROVIDER_ROUTINES));
    PCHAR pSearchEnd = (PCHAR)pNtKernelData + uNtKernelDataSize;

    PCHAR pLxCoreModuleStart = (PCHAR)hdlLxCore;
    PCHAR pLxCoreModuleEnd = pLxCoreModuleStart + uLxCoreSize;

    const auto CheckLxCoreAddress = [&](PVOID ptr) -> bool
    {
        return pLxCoreModuleStart <= (PCHAR)ptr &&
            (PCHAR)ptr < pLxCoreModuleEnd;
    };

    for (; pSearchStart < pSearchEnd; pSearchStart += alignof(PS_PICO_PROVIDER_ROUTINES))
    {
        PPS_PICO_PROVIDER_ROUTINES pTestRoutines = (PPS_PICO_PROVIDER_ROUTINES)pSearchStart;

        // Check size.

        if (pTestRoutines->Size > sizeof(PPS_PICO_PROVIDER_ROUTINES) * 16)
        {
            // The struct may grow over time, but it is unlikely to grow that large.
            continue;
        }

        if (pSearchStart + pTestRoutines->Size > pSearchEnd)
        {
            // Otherwise, we cannot read the rest of the structure.
            continue;
        }

        // Check routines
        if (!CheckLxCoreAddress(pTestRoutines->DispatchSystemCall))
        {
            continue;
        }

        if (!CheckLxCoreAddress(pTestRoutines->ExitThread))
        {
            continue;
        }

        if (!CheckLxCoreAddress(pTestRoutines->ExitProcess))
        {
            continue;
        }

        if (!CheckLxCoreAddress(pTestRoutines->DispatchException))
        {
            continue;
        }

        if (!CheckLxCoreAddress(pTestRoutines->TerminateProcess))
        {
            continue;
        }

        if (!CheckLxCoreAddress(pTestRoutines->WalkUserStack))
        {
            continue;
        }

        // TODO: Is this supposed to be inside the driver?
        if (!CheckLxCoreAddress((PVOID)pTestRoutines->ProtectedRanges))
        {
            continue;
        }

        // This member is NULL in newer Windows versions
        // (Tested on Windows 11 23H2)
        if (pTestRoutines->GetAllocatedProcessImageName != NULL &&
            !CheckLxCoreAddress(pTestRoutines->GetAllocatedProcessImageName))
        {
            continue;
        }

        // TODO: Check OpenProcess and OpenThread.
        // TODO: The last unknown member of the struct is always 1. Check for that?

        // The current routines match the criteria. Store it.
        PspPicoProviderRoutines = pTestRoutines;

        Logger::LogTrace("PspPicoProviderRoutines found at ", pTestRoutines);

        *pPpr = pTestRoutines;

        return STATUS_SUCCESS;
    }

    Logger::LogError("Cannot find Pico provider routines.");
    Logger::LogError("Make sure WSL is enabled and LXCORE.SYS is loaded.");

    return STATUS_NOT_FOUND;
}

extern "C"
NTSTATUS
PicoSppLocateRoutines(
    _Out_ PPS_PICO_ROUTINES* pPr
)
{
    if (pPr == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = STATUS_SUCCESS;

    HANDLE hdlLxCore = NULL;
    SIZE_T uLxCoreSize = 0;
    status = MdlpFindModuleByName("lxcore.sys", &hdlLxCore, &uLxCoreSize);

    Logger::LogTrace("Find lxcore: status=", (void*)status, " handle=", hdlLxCore,
        " size=", uLxCoreSize);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    PVOID pLxpRoutines;
    status = MdlpGetProcAddress(hdlLxCore, "LxpRoutines", &pLxpRoutines);

    Logger::LogTrace("Find LxpRoutines: status=", (void*)status, " &LxpRoutines=", pLxpRoutines);

    if (!NT_SUCCESS(status))
    {
        Logger::LogError("Cannot find Pico routines.");
        Logger::LogError("Make sure WSL is enabled and LXCORE.SYS is loaded.");
        return status;
    }

    *pPr = (PPS_PICO_ROUTINES)pLxpRoutines;

    return STATUS_SUCCESS;
}
