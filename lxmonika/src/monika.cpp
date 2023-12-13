#include "monika.h"

#include "os.h"
#include "picosupport.h"

#include "Logger.h"
#include "ProcessMap.h"

//
// Monika data
//

PS_PICO_PROVIDER_ROUTINES MapOriginalProviderRoutines;
PS_PICO_ROUTINES MapOriginalRoutines;

PS_PICO_PROVIDER_ROUTINES MapProviderRoutines[MaPicoProviderMaxCount];
PS_PICO_ROUTINES MapRoutines[MaPicoProviderMaxCount];

CHAR MapProviderNames[MaPicoProviderMaxCount][MA_NAME_MAX + 1];

ProcessMap MapProcessMap;

SIZE_T MapProvidersCount = 0;

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

    memcpy(&MapOriginalRoutines, pRoutines, sizeof(MapOriginalRoutines));

    if (pRoutines->Size != sizeof(MapOriginalRoutines))
    {
        Logger::LogWarning("Expected size ", sizeof(MapOriginalRoutines),
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

    // Initialize process map
    MapProcessMap.Initialize();

    // /dev/reality
    status = MapInitializeLxssDevice();

    Logger::LogTrace("MapInitializeLxssDevice status=", (void*)status);

    if (!NT_SUCCESS(status))
    {
        Logger::LogWarning("Failed to initialize lxss device. "
            "WSL integration features will not work.");
        Logger::LogWarning("Make sure WSL1 is enabled and lxcore.sys is loaded.");
    }

    return STATUS_SUCCESS;
}

extern "C"
VOID
MapCleanup()
{
    PPS_PICO_PROVIDER_ROUTINES pRoutines = NULL;
    if (NT_SUCCESS(PicoSppLocateProviderRoutines(&pRoutines)))
    {
        memcpy(pRoutines, &MapOriginalProviderRoutines, sizeof(MapOriginalProviderRoutines));
    }
    MapProcessMap.Clear();
    MapCleanupLxssDevice();
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
    MapRoutines[uProviderIndex] = *PicoRoutines;

    return STATUS_SUCCESS;
}

extern "C"
MONIKA_EXPORT
NTSTATUS NTAPI
MaSetPicoProviderName(
    _In_ PVOID ProviderDetails,
    _In_ PCSTR Name
)
{
    // Ignore any potential increments of MapProvidersCount by MaRegisterPicoProvider.
    // We cannot set a provider's name and then register it!
    SIZE_T uCurrentProvidersCount = MapProvidersCount;

    // MapProvidersCount can briefly exceed the max count when drivers are trying to register a
    // 17th provider. Using min should protect us from this issue.
    uCurrentProvidersCount = min(uCurrentProvidersCount, MaPicoProviderMaxCount);

    if ((*(PSIZE_T)ProviderDetails) != sizeof(PS_PICO_PROVIDER_ROUTINES))
    {
        // We currently only support passing the PS_PICO_PROVIDER_ROUTINES struct. In future
        // versions, we may support using an opaque handle or the provider index itself.
        return STATUS_INVALID_PARAMETER;
    }

    for (SIZE_T i = 0; i < uCurrentProvidersCount; ++i)
    {
        if (memcmp(ProviderDetails, &MapProviderRoutines[i],
            sizeof(PS_PICO_PROVIDER_ROUTINES)) == 0)
        {
            strncpy(MapProviderNames[i], Name, MA_NAME_MAX);
            return STATUS_SUCCESS;
        }
    }

    return STATUS_NOT_FOUND;
}
