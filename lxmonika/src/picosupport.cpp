#include "picosupport.h"

#include <ntddk.h>
#include <wdm.h>

#include "module.h"

#include "Logger.h"

static PPS_PICO_PROVIDER_ROUTINES PspPicoProviderRoutines = NULL;
static PS_PICO_ROUTINES PspPicoRoutines
{
    .Size = 0
};

static
VOID
PicoSpStringUnicodeToAnsi(
    _Out_ PSTR pAnsiString,
    _In_ PCWSTR pUnicodeString,
    _In_ SIZE_T uMaxLen
)
{
    SIZE_T uLen = wcslen(pUnicodeString);

    uLen = min(uLen, uMaxLen - 1);

    // Copy the null char as well.
    for (SIZE_T i = 0; i <= uLen; ++i)
    {
        pAnsiString[i] = (CHAR)pUnicodeString[i];
    }
}

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

    PCWSTR pVersionInfoStringUnicode;
    SIZE_T uVersionInfoStringBytes = uNtKernelSize;
    status = MdlpGetProductVersion(hdlNtKernel,
        (PCWSTR*)&pVersionInfoStringUnicode, &uVersionInfoStringBytes);

    if (NT_SUCCESS(status))
    {
        CHAR pVersionInfoStringAnsi[32];
        PicoSpStringUnicodeToAnsi(pVersionInfoStringAnsi, pVersionInfoStringUnicode,
            sizeof(pVersionInfoStringAnsi));

        Logger::LogTrace("Detected Windows NT version ", pVersionInfoStringAnsi);

        PMA_PSP_PICO_PROVIDER_ROUTINES_OFFSETS pOffsets = NULL;
        status = PicoSppGetOffsets(pVersionInfoStringAnsi, NULL, &pOffsets);

        if (NT_SUCCESS(status) && pOffsets->Offsets.PspPicoProviderRoutines != 0)
        {
            PPS_PICO_PROVIDER_ROUTINES pMaybeTheRightRoutines =
                (PPS_PICO_PROVIDER_ROUTINES)
                    ((PCHAR)hdlNtKernel + pOffsets->Offsets.PspPicoProviderRoutines);

            Logger::LogTrace("PspPicoProviderRoutines found at ", pMaybeTheRightRoutines);

            // Do a size check first. This reduces the chance of wrong version handling code
            // bootlooping Windows.
            //
            // It might be a good idea to check for the pointers in Lxss as well, however, this
            // would defeat the purpose of using known offsets: To support situations where
            // other drivers have patched the routines beforehand.
            //
            // ExitThread is chosen because it is the member right after DispatchSystemCall.
            // DispatchSystemCall is absolutely necessary for any Pico provider to function.

            if (pMaybeTheRightRoutines->Size <
                FIELD_OFFSET(PS_PICO_PROVIDER_ROUTINES, ExitThread)
                || pMaybeTheRightRoutines->Size > sizeof(PS_PICO_PROVIDER_ROUTINES) * 16)
            {
                Logger::LogWarning("Disregarding known offset due to size being suspicious: ",
                    pMaybeTheRightRoutines->Size);
            }
            else
            {
                *pPpr = PspPicoProviderRoutines = pMaybeTheRightRoutines;
                return STATUS_SUCCESS;
            }
        }
        else
        {
            Logger::LogWarning("Windows NT version ", pVersionInfoStringAnsi,
                " is not supported.");
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

        if ((pTestRoutines->OpenProcess & (~PROCESS_ALL_ACCESS)) != 0)
        {
            continue;
        }

        if ((pTestRoutines->OpenThread & (~THREAD_ALL_ACCESS)) != 0)
        {
            continue;
        }

        if (pTestRoutines->SubsystemInformationType != SubsystemInformationTypeWSL)
        {
            continue;
        }

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

    // Method 1: Resolve symbols from lxcore.sys
    HANDLE hdlLxCore = NULL;
    SIZE_T uLxCoreSize = 0;
    status = MdlpFindModuleByName("lxcore.sys", &hdlLxCore, &uLxCoreSize);

    Logger::LogTrace("Find lxcore: status=", (void*)status, " handle=", hdlLxCore,
        " size=", uLxCoreSize);

    if (NT_SUCCESS(status))
    {
        PVOID pLxpRoutines;
        status = MdlpGetProcAddress(hdlLxCore, "LxpRoutines", &pLxpRoutines);

        Logger::LogTrace("Find LxpRoutines: status=", (void*)status, " &LxpRoutines=",
            pLxpRoutines);

        if (!NT_SUCCESS(status))
        {
            goto fail;
        }

        *pPr = (PPS_PICO_ROUTINES)pLxpRoutines;
        return STATUS_SUCCESS;
    }

    // Method 2: Known offsets from the NT Kernel base.

    // Cached value
    if (PspPicoRoutines.Size != 0)
    {
        *pPr = &PspPicoRoutines;
        return STATUS_SUCCESS;
    }

    HANDLE hdlNtKernel;
    SIZE_T uNtKernelSize;
    status = MdlpFindModuleByName("ntoskrnl.exe", &hdlNtKernel, &uNtKernelSize);

    Logger::LogTrace("Find ntoskrnl: status=", (PVOID)status, " handle=", hdlNtKernel,
        " size=", uNtKernelSize);

    if (NT_SUCCESS(status))
    {
        PCWSTR pVersionInfoStringUnicode;
        CHAR pVersionInfoStringAnsi[32];
        SIZE_T uVersionInfoStringBytes = uNtKernelSize;
        status = MdlpGetProductVersion(hdlNtKernel,
            (PCWSTR*)&pVersionInfoStringUnicode, &uVersionInfoStringBytes);

        if (!NT_SUCCESS(status))
        {
            goto fail;
        }

        PicoSpStringUnicodeToAnsi(pVersionInfoStringAnsi, pVersionInfoStringUnicode,
            sizeof(pVersionInfoStringAnsi));

        Logger::LogTrace("Detected Windows NT version ", pVersionInfoStringAnsi);

        PMA_PSP_PICO_PROVIDER_ROUTINES_OFFSETS pOffsets = NULL;
        status = PicoSppGetOffsets(pVersionInfoStringAnsi, NULL, &pOffsets);

        if (!NT_SUCCESS(status))
        {
            Logger::LogWarning("Windows NT version ", pVersionInfoStringAnsi,
                " is not supported.");
            goto fail;
        }

#define PICO_SPP_CHECK_AND_ASSIGN(member, symbol)                                               \
        if (pOffsets->Offsets.symbol == 0)                                                      \
        {                                                                                       \
            Logger::LogWarning("The offset to " #symbol " is unknown.");                        \
            /* Actually happens on 16xxx ARM builds. */                                         \
            Logger::LogWarning("Maybe Microsoft has blocked Pico processed on this build?");    \
            goto fail;                                                                          \
        }                                                                                       \
        else                                                                                    \
        {                                                                                       \
            PspPicoRoutines.member = (decltype(PspPicoRoutines.member))                         \
                ((PCHAR)hdlNtKernel + pOffsets->Offsets.symbol);                                \
        }

        PICO_SPP_CHECK_AND_ASSIGN(CreateProcess,            PspCreatePicoProcess);
        PICO_SPP_CHECK_AND_ASSIGN(CreateThread,             PspCreatePicoThread);
        PICO_SPP_CHECK_AND_ASSIGN(GetProcessContext,        PspGetPicoProcessContext);
        PICO_SPP_CHECK_AND_ASSIGN(GetThreadContext,         PspGetPicoThreadContext);
        PICO_SPP_CHECK_AND_ASSIGN(GetContextThreadInternal, PspPicoGetContextThreadEx);
        PICO_SPP_CHECK_AND_ASSIGN(SetContextThreadInternal, PspPicoSetContextThreadEx);
        PICO_SPP_CHECK_AND_ASSIGN(TerminateThread,          PspTerminateThreadByPointer);
        PICO_SPP_CHECK_AND_ASSIGN(ResumeThread,             PsResumeThread);
        PICO_SPP_CHECK_AND_ASSIGN(SetThreadDescriptorBase,  PspSetPicoThreadDescriptorBase);
        PICO_SPP_CHECK_AND_ASSIGN(SuspendThread,            PsSuspendThread);
        PICO_SPP_CHECK_AND_ASSIGN(TerminateProcess,         PspTerminatePicoProcess);

#undef PICO_SPP_CHECK_AND_ASSIGN

        PspPicoRoutines.Size = sizeof(PS_PICO_ROUTINES);
        *pPr = &PspPicoRoutines;

        return STATUS_SUCCESS;
    }


fail:
    Logger::LogError("Cannot find Pico routines.");
    Logger::LogError("Make sure WSL is enabled and LXCORE.SYS is loaded.");
    return status;
}

extern "C"
NTSTATUS
PicoSppGetOffsets(
    _In_ PCSTR pVersion,
    _In_opt_ PCSTR pArchitecture,
    _Out_ PMA_PSP_PICO_PROVIDER_ROUTINES_OFFSETS* pPOffsets
)
{
    if (pVersion == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (pArchitecture == NULL)
    {
#ifdef _M_X64
        pArchitecture = "x64";
#elif defined(_M_ARM64)
        pArchitecture = "arm64";
#else
#error Define the identifier for this architecture!
#endif
    }

    Logger::LogTrace("Finding offsets for Windows version ", pVersion,
        " of architecture ", pArchitecture);

    for (SIZE_T i = 0; i < ARRAYSIZE(MaPspPicoProviderRoutinesOffsets); ++i)
    {
        PMA_PSP_PICO_PROVIDER_ROUTINES_OFFSETS pCurrentOffsets =
            (PMA_PSP_PICO_PROVIDER_ROUTINES_OFFSETS)&MaPspPicoProviderRoutinesOffsets[i];

        if (pCurrentOffsets->Version == NULL)
        {
            Logger::LogTrace("Wait, why is the version NULL? (i=", i, ")");
            continue;
        }

        if (pCurrentOffsets->Architecture == NULL)
        {
            Logger::LogTrace("Wait, why is the architecture NULL? (i=", i, ")");
            continue;
        }

        if (strcmp(pVersion, pCurrentOffsets->Version) == 0
            && strcmp(pArchitecture, pCurrentOffsets->Architecture) == 0)
        {
            *pPOffsets = pCurrentOffsets;
            return STATUS_SUCCESS;
        }
    }

    Logger::LogInfo("Failed to find suitable offsets for this Windows version.");
    Logger::LogInfo("Maybe it's time to update these offsets.");
    Logger::LogInfo("See https://github.com/trungnt2910/PspPicoProviderRoutinesOffsetGen");

    return STATUS_NOT_FOUND;
}
