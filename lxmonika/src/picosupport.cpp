#include "picosupport.h"

#include <ntddk.h>
#include <wdm.h>

#include "module.h"

#include "AutoResource.h"
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

        if (pTestRoutines->SubsystemInformationType != 1 /* SubsystemInformationTypeWSL */)
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
#elif defined(_M_IX86)
        pArchitecture = "x86";
#elif defined(_M_ARM)
        pArchitecture = "arm";
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

extern "C"
NTSTATUS
PicoSppDetermineAbiStatus(
    _Out_ PSIZE_T pProviderRoutinesSize,
    _Out_ PSIZE_T pPicoRoutinesSize,
    _Out_ DWORD* pAbiVersion,
    _Out_ PBOOLEAN pHasSizeChecks,
    _Out_ PBOOLEAN pTooLate
)
{
    NTSTATUS status;

    PS_PICO_PROVIDER_ROUTINES psTestProviderRoutines
    {
        .Size = 0
    };
    PS_PICO_ROUTINES psTestRoutines
    {
        .Size = 0
    };

    // Test pages for triggering exceptions.
    // Using MM_BAD_POINTER will only trigger BSODs.
    PVOID pTestPages = NULL;
    SIZE_T szTestPagesSize = sizeof(PS_PICO_PROVIDER_ROUTINES) + PAGE_SIZE;

    status = ZwAllocateVirtualMemory(
        ZwCurrentProcess(),
        &pTestPages,
        0,
        &szTestPagesSize,
        MEM_RESERVE,
        PAGE_NOACCESS
    );
    if (!NT_SUCCESS(status))
    {
        Logger::LogError("Failed to reserve virtual memory for testing.");
        return status;
    }

    AUTO_RESOURCE(pTestPages,
        [](auto p)
        {
            SIZE_T szZero = 0;
            ZwFreeVirtualMemory(ZwCurrentProcess(), &p, &szZero, MEM_RELEASE);
        }
    );

    // Check if PsRegisterPicoProvider is a stub.
    __try
    {
        status = PsRegisterPicoProvider(
            (PPS_PICO_PROVIDER_ROUTINES)pTestPages,
            (PPS_PICO_ROUTINES)pTestPages
        );

        // On TH2, the function ignores both parameters and returns a STATUS_TOO_LATE.
        // The access violation should not happen.

        // This also happens when we're "too late" in some older builds without size checks,
        // but we cannot do much more in those cases either. We do not have offset databases
        // for these early builds, and lxcore did not export the LxpRoutines symbol at that time.

        *pProviderRoutinesSize = (SIZE_T)-1;
        *pPicoRoutinesSize = (SIZE_T)-1;
        *pAbiVersion = NTDDI_WIN10_TH2;
        *pHasSizeChecks = FALSE;
        *pTooLate = TRUE;

        return STATUS_NOT_SUPPORTED;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // Expected to reach here.
    }

    status = PsRegisterPicoProvider(&psTestProviderRoutines, &psTestRoutines);

    if (NT_SUCCESS(status))
    {
        // Ancient builds of Windows (before 10128) where the size checks are not done.
        *pAbiVersion = NTDDI_WIN10;
        *pHasSizeChecks = FALSE;

        // If it's actually too late, it has already been caught in the block above.
        *pTooLate = FALSE;

        SIZE_T szTestPagesWritableSize = sizeof(PS_PICO_PROVIDER_ROUTINES);
        status = ZwAllocateVirtualMemory(
            ZwCurrentProcess(),
            &pTestPages,
            0,
            &szTestPagesWritableSize,
            MEM_COMMIT,
            PAGE_READWRITE
        );
        if (!NT_SUCCESS(status))
        {
            Logger::LogError("Failed to commit virtual memory for testing.");
            return status;
        }

        BOOLEAN found = FALSE;

        for (SIZE_T szTestSize = 0;
            szTestSize <= sizeof(PS_PICO_PROVIDER_ROUTINES);
            szTestSize += sizeof(PVOID))
        {
            PVOID pStartAddress = (PCHAR)pTestPages + szTestPagesWritableSize - szTestSize;

            __try
            {
                // zero out this struct since we will use the results later.
                memset(&psTestRoutines, 0, sizeof(psTestRoutines));

                status = PsRegisterPicoProvider(
                    (PPS_PICO_PROVIDER_ROUTINES)pStartAddress,
                    &psTestRoutines
                );
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                // szTestSize is too small. The system would try to read into uncommitted memory.
                continue;
            }

            *pProviderRoutinesSize = szTestSize;

            if (!NT_SUCCESS(status))
            {
                Logger::LogWarning("Unexpected error code: ", (PVOID)status);
            }

            break;
        }

        if (!found)
        {
            Logger::LogError("The system Pico provider routines size is larger than expected.");
            Logger::LogError("This cannot happen since this should be an older Windows build.");
            Logger::LogError("Maybe there's a bug in lxmonika?");
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        // The routines' size is the number of non-null fields times the size of a pointer
        // (since all the fields are pointers).
        PVOID* pBegin = (PVOID*)&psTestRoutines;
        PVOID* pEnd = (PVOID*)(((PCHAR)&psTestRoutines) + sizeof(PS_PICO_ROUTINES));

        while (pEnd != pBegin && pEnd[-1] == NULL)
        {
            --pEnd;
        }

        *pPicoRoutinesSize = (pEnd - pBegin) * sizeof(PVOID);

        return STATUS_SUCCESS;
    }
    else if (status == STATUS_INFO_LENGTH_MISMATCH)
    {
        // The size checks are done. Exploit that to determine the sizes.
        *pHasSizeChecks = TRUE;

        BOOLEAN found = FALSE;

        // Decrease the first struct size until the system starts to read the second struct.
        for (SIZE_T szTestSize = sizeof(PS_PICO_PROVIDER_ROUTINES);
            szTestSize > 0;
            szTestSize -= sizeof(PVOID))
        {
            __try
            {
                psTestProviderRoutines.Size = szTestSize;
                status = PsRegisterPicoProvider(
                    &psTestProviderRoutines,
                    (PPS_PICO_ROUTINES)pTestPages
                );
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                // Access violation occurred, we have the correct size.
                *pProviderRoutinesSize = szTestSize;
                found = TRUE;
                break;
            }
        }

        if (!found)
        {
            // We might be on a newer Windows version, where the first struct is larger?
            Logger::LogError("The system Pico provider routines size is larger than expected.");
            Logger::LogError("Probably this copy of lxmonika is outdated.");

            return STATUS_INFO_LENGTH_MISMATCH;
        }

        // Set these masks to avoid failing parameter checks.
        psTestProviderRoutines.OpenProcess = PROCESS_ALL_ACCESS;
        psTestProviderRoutines.OpenThread = THREAD_ALL_ACCESS;

        found = FALSE;

        // Decrease the second struct size until we pass the checks.
        for (SIZE_T szTestSize = sizeof(PS_PICO_ROUTINES);
            szTestSize > 0;
            szTestSize -= sizeof(PVOID))
        {
            psTestRoutines.Size = szTestSize;
            status = PsRegisterPicoProvider(&psTestProviderRoutines, &psTestRoutines);

            if (status != STATUS_INFO_LENGTH_MISMATCH)
            {
                // We got the correct size.
                *pPicoRoutinesSize = szTestSize;
                found = TRUE;

                Logger::LogTrace(
                    "The correct sizes are: ", *pProviderRoutinesSize, ", ", *pPicoRoutinesSize);
                Logger::LogTrace("The return status is: ", (PVOID)status);

                if (!NT_SUCCESS(status))
                {
                    if (status != STATUS_TOO_LATE)
                    {
                        Logger::LogWarning("Got an unexpected error code: ", (PVOID)status);
                    }
                    *pTooLate = TRUE;
                }
                else
                {
                    *pTooLate = FALSE;
                }

                break;
            }
        }

        if (!found)
        {
            // We might be on a newer Windows version, where the second struct is larger?
            Logger::LogError("The system Pico routines size is larger than expected.");
            Logger::LogError("Probably this copy of lxmonika is outdated.");

            return STATUS_INFO_LENGTH_MISMATCH;
        }

        return PicoSppDetermineAbiVersion(
            *pProviderRoutinesSize,
            *pPicoRoutinesSize,
            pAbiVersion
        );
    }
    else
    {
        // Some other fail status but not related to our Pico structures?
        DbgBreakPoint();
        return STATUS_NOT_SUPPORTED;
    }
}

extern "C"
NTSTATUS
PicoSppDetermineAbiVersion(
    _In_ SIZE_T szProviderRoutines,
    _In_ SIZE_T szPicoRoutines,
    _Out_ DWORD* pAbiVersion
)
{
    if (szProviderRoutines > sizeof(PS_PICO_PROVIDER_ROUTINES)
        || szPicoRoutines > sizeof(PS_PICO_ROUTINES))
    {
        // A driver designed for a newer Windows version?
        return STATUS_NOT_SUPPORTED;
    }

    // TODO: There is a gap between build 14347
    // (the first build to introduce the additional members)
    // and the actual build where the functions have their signatures changed
    // (somewhere before 14393 - the RS1 release).
    //
    // These are very rare Insider builds that lxmonika will probably never run on.

    if (szProviderRoutines > FIELD_OFFSET(PS_PICO_PROVIDER_ROUTINES, OpenProcess))
    {
        *pAbiVersion = NTDDI_WIN10_RS1;
    }
    else
    {
        *pAbiVersion = NTDDI_WIN10;
    }

    return STATUS_SUCCESS;
}
