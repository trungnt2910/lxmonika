#include "monika.h"

#include "module.h"
#include "os.h"
#include "picosupport.h"

#include "Logger.h"

//
// Monika data
//

PS_PICO_PROVIDER_ROUTINES MapOriginalProviderRoutines;
PS_PICO_ROUTINES MapOriginalRoutines;

PS_PICO_PROVIDER_ROUTINES MapProviderRoutines[MaPicoProviderMaxCount];
PS_PICO_ROUTINES MapRoutines[MaPicoProviderMaxCount];
MA_PICO_PROVIDER_ROUTINES MapAdditionalProviderRoutines[MaPicoProviderMaxCount];
MA_PICO_ROUTINES MapAdditionalRoutines[MaPicoProviderMaxCount];
SIZE_T MapProvidersCount = 0;

BOOLEAN MapPatchedLxss = FALSE;

static LONG MaInitialized = FALSE;

//
// Monika lifetime functions
//

extern "C"
NTSTATUS
MapInitialize()
{
    if (InterlockedCompareExchange(&MaInitialized, TRUE, FALSE))
    {
        // Already initialized for most of the time.
        // The only time that Monika may not be initialized yet is during boot where Windows calls
        // the DriverEntry functions for each registered driver.
        // During this time, driver initialization should happen sequentially, so this function
        // cannot be called at the same time by two threads.
        // If I am mistaken (I might be, since I am a KMDF noob after all), then we might need an
        // actual lock here.
        return STATUS_SUCCESS;
    }

    NTSTATUS status;

    PPS_PICO_ROUTINES pRoutines = NULL;
    PPS_PICO_PROVIDER_ROUTINES pProviderRoutines = NULL;

    status = PicoSppLocateRoutines(&pRoutines);

    Logger::LogTrace("PicoSppLocateRoutines status=", (void*)status);

    if (!NT_SUCCESS(status))
    {
        goto fail;
    }

    memcpy(&MapOriginalRoutines, pRoutines, sizeof(MapOriginalRoutines));

    if (pRoutines->Size != sizeof(MapOriginalRoutines))
    {
        Logger::LogWarning("Expected size ", sizeof(MapOriginalRoutines),
            " for struct PS_PICO_ROUTINES, but got ", pRoutines->Size);
        Logger::LogWarning("Please update the pico struct definitions.");
    }

    status = PicoSppLocateProviderRoutines(&pProviderRoutines);

    Logger::LogTrace("PicoSppLocateProviderRoutines status=", (void*)status);

    if (!NT_SUCCESS(status))
    {
        goto fail;
    }

    memcpy(&MapOriginalProviderRoutines, pProviderRoutines, sizeof(MapOriginalProviderRoutines));

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
    MapRoutines[MaPicoProvider##index] =                                    \
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

    // Initialize additional pico routines
#define MONIKA_PROVIDER(index)                                              \
    MapAdditionalRoutines[MaPicoProvider##index] =                          \
    {                                                                       \
        .Size = sizeof(MA_PICO_ROUTINES)                                    \
    };
#include "monika_providers.cpp"
#undef MONIKA_PROVIDER

    // Optional step: Register WSL as a lxmonika client to simplify Pico process routines handling.
    HANDLE hdlLxCore;
    if (NT_SUCCESS(MdlpFindModuleByName("lxcore.sys", &hdlLxCore, NULL)))
    {
        PPS_PICO_ROUTINES pLxpRoutines = NULL;
        status = MdlpGetProcAddress(hdlLxCore, "LxpRoutines", (PVOID*)&pLxpRoutines);

        if (NT_SUCCESS(status))
        {
            // This function should be called before any MaRegisterPicoProvider calls succeeds.
            ASSERT(MapProvidersCount == 0);
            ++MapProvidersCount;
            ASSERT(MapProvidersCount == 1);

            Logger::LogTrace("lxcore.sys detected. Found LxpRoutines at ", pLxpRoutines);

            // Should be safe to patch everything here since lxcore.sys, as a core driver,
            // should be initialized before third-party ones.
            MapProviderRoutines[0] = MapOriginalProviderRoutines;
            *pLxpRoutines = MapRoutines[0];

            // Use a special hook designed for WSL instead of the generic lxmonika shims.
            MapProviderRoutines[0].DispatchSystemCall = MapLxssSystemCallHook;

            MapAdditionalProviderRoutines[0].GetAllocatedProviderName =
                MapLxssGetAllocatedProviderName;
            MapAdditionalProviderRoutines[0].GetConsole = MapLxssGetConsole;

            MapPatchedLxss = TRUE;

            Logger::LogTrace("lxcore.sys successfully registered as a lxmonika provider.");
        }
    }

    return STATUS_SUCCESS;

fail:
    MaInitialized = FALSE;
    return status;
}

extern "C"
VOID
MapCleanup()
{
    if (InterlockedCompareExchange(&MaInitialized, FALSE, TRUE))
    {
        PPS_PICO_PROVIDER_ROUTINES pRoutines = NULL;
        if (NT_SUCCESS(PicoSppLocateProviderRoutines(&pRoutines)))
        {
            *pRoutines = MapOriginalProviderRoutines;
        }
        if (MapPatchedLxss)
        {
            PPS_PICO_ROUTINES pLxpRoutines = NULL;
            HANDLE hdlLxCore;
            if (NT_SUCCESS(MdlpFindModuleByName("lxcore.sys", &hdlLxCore, NULL))
                && NT_SUCCESS(MdlpGetProcAddress(hdlLxCore, "LxpRoutines", (PVOID*)&pLxpRoutines)))
            {
                *pLxpRoutines = MapOriginalRoutines;
            }
            MapPatchedLxss = FALSE;
        }
    }
}


//
// Pico provider registration
//

extern "C"
MONIKA_EXPORT
NTSTATUS NTAPI
MaRegisterPicoProvider(
    _In_ PPS_PICO_PROVIDER_ROUTINES ProviderRoutines,
    _Inout_ PPS_PICO_ROUTINES PicoRoutines
)
{
    return MaRegisterPicoProviderEx(ProviderRoutines, PicoRoutines, NULL, NULL, NULL);
}

extern "C"
MONIKA_EXPORT
NTSTATUS NTAPI
MaRegisterPicoProviderEx(
    _In_ PPS_PICO_PROVIDER_ROUTINES ProviderRoutines,
    _Inout_ PPS_PICO_ROUTINES PicoRoutines,
    _In_opt_ PMA_PICO_PROVIDER_ROUTINES AdditionalProviderRoutines,
    _Inout_opt_ PMA_PICO_ROUTINES AdditionalPicoRoutines,
    _Out_opt_ PSIZE_T Index
)
{
    if (ProviderRoutines->Size != sizeof(PS_PICO_PROVIDER_ROUTINES)
        || PicoRoutines->Size != sizeof(PS_PICO_ROUTINES))
    {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    if ((ProviderRoutines->OpenProcess & (~PROCESS_ALL_ACCESS)) != 0
        || (ProviderRoutines->OpenThread & (~THREAD_ALL_ACCESS)) != 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // BE CAREFUL! This function might be called by other drivers before lxmonika's own DriverEntry
    // gets called.
    NTSTATUS status = MapInitialize();
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    // Acquire an index for the provider.
    // Once a provider has been registered, it cannot be unregistered.
    // This is the same as the NT kernel as PsUnregisterPicoProvider does not exist.
    SIZE_T uProviderIndex = InterlockedIncrementSizeT(&MapProvidersCount) - 1;
    if (uProviderIndex >= MaPicoProviderMaxCount)
    {
        // Prevent SIZE_T overflow (quite hard on 64-bit systems).
        InterlockedDecrementSizeT(&MapProvidersCount);

        // PsRegisterPicoProvider would return STATUS_TOO_LATE here.
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    MapProviderRoutines[uProviderIndex] = *ProviderRoutines;
    *PicoRoutines = MapRoutines[uProviderIndex];

    // Allows compatibility between different versions of lxmonika.
    // (Hopefully, at least when the API becomes stable).
    if (AdditionalProviderRoutines != NULL)
    {
        memcpy(&MapAdditionalProviderRoutines[uProviderIndex], AdditionalProviderRoutines,
            min(sizeof(MA_PICO_PROVIDER_ROUTINES), AdditionalProviderRoutines->Size));
    }

    if (AdditionalPicoRoutines != NULL)
    {
        memcpy(AdditionalPicoRoutines, &MapAdditionalRoutines[uProviderIndex],
            min(sizeof(MA_PICO_ROUTINES), AdditionalPicoRoutines->Size));
    }

    if (Index != NULL)
    {
        *Index = uProviderIndex;
    }

    return STATUS_SUCCESS;
}

extern "C"
MONIKA_EXPORT
NTSTATUS NTAPI
MaFindPicoProvider(
    _In_ PCWSTR ProviderName,
    _Out_ PSIZE_T Index
)
{
    if (ProviderName == NULL || Index == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Ignore any potential increments of MapProvidersCount by MaRegisterPicoProvider.
    // We cannot set a provider's name and then register it!
    SIZE_T uCurrentProvidersCount = MapProvidersCount;

    // MapProvidersCount can briefly exceed the max count when drivers are trying to register a
    // 17th provider. Using min should protect us from this issue.
    uCurrentProvidersCount = min(uCurrentProvidersCount, MaPicoProviderMaxCount);

    SIZE_T uNameLenBytes = (wcslen(ProviderName) + 1) * sizeof(WCHAR);

    SIZE_T uBestMatchLength = 0;
    SIZE_T uBestMatchIndex = 0;

    for (SIZE_T i = 0; i < uCurrentProvidersCount; ++i)
    {
        if (MapAdditionalProviderRoutines[i].GetAllocatedProviderName != NULL)
        {
            PUNICODE_STRING pName = NULL;
            if (!NT_SUCCESS(MapAdditionalProviderRoutines[i].GetAllocatedProviderName(&pName)))
            {
                continue;
            }
            SIZE_T uCurrentMatch = RtlCompareMemory(pName->Buffer, ProviderName,
                min(pName->Length, uNameLenBytes));

            if (uCurrentMatch > uBestMatchLength)
            {
                uBestMatchLength = uCurrentMatch;
                uBestMatchIndex = i;
            }
        }
    }

    if (uBestMatchLength == 0)
    {
        return STATUS_NOT_FOUND;
    }

    *Index = uBestMatchIndex;
    return STATUS_SUCCESS;
}

#define MA_CALL_IF_SUPPORTED(index, function, ...)                                              \
    do                                                                                          \
    {                                                                                           \
        if (MapAdditionalProviderRoutines[index].function == NULL)                              \
        {                                                                                       \
            return STATUS_INVALID_PARAMETER;                                                    \
        }                                                                                       \
        return MapAdditionalProviderRoutines[index].function(__VA_ARGS__);                      \
    }                                                                                           \
    while (TRUE);

MONIKA_EXPORT
NTSTATUS NTAPI
MaGetAllocatedPicoProviderName(
    _In_ SIZE_T Index,
    _Out_ PUNICODE_STRING* ProviderName
)
{
    if (Index >= MaPicoProviderMaxCount || Index >= MapProvidersCount
        || ProviderName == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    MA_CALL_IF_SUPPORTED(Index, GetAllocatedProviderName, ProviderName);
}

MONIKA_EXPORT
NTSTATUS NTAPI
MaStartSession(
    _In_ SIZE_T Index,
    _In_ PMA_PICO_SESSION_ATTRIBUTES SessionAttributes
)
{
    if (Index >= MaPicoProviderMaxCount || Index >= MapProvidersCount
        || SessionAttributes == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    MA_CALL_IF_SUPPORTED(Index, StartSession, SessionAttributes);
}

MONIKA_EXPORT
NTSTATUS NTAPI
MaGetConsole(
    _In_ PEPROCESS Process,
    _Out_opt_ PHANDLE Console,
    _Out_opt_ PHANDLE Input,
    _Out_opt_ PHANDLE Output
)
{
    PMA_CONTEXT pContext = NULL;
    MA_RETURN_IF_FAIL(MapGetObjectContext(Process, &pContext));

    MA_CALL_IF_SUPPORTED(pContext->Provider, GetConsole, Process, Console, Input, Output);
}

MONIKA_EXPORT
NTSTATUS NTAPI
MaUtilDuplicateKernelHandle(
    _In_ HANDLE SourceHandle,
    _Out_ PHANDLE TargetHandle
)
{
    // Any process handle should work if we are dealing with
    // kernel object handles.
    return ZwDuplicateObject(
        ZwCurrentProcess(),
        SourceHandle,
        ZwCurrentProcess(),
        TargetHandle,
        0,
        OBJ_KERNEL_HANDLE,
        DUPLICATE_SAME_ACCESS
    );
}
