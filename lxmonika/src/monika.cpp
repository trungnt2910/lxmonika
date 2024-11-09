#include "monika.h"

#include "os.h"
#include "picosupport.h"

#include "AutoResource.h"
#include "Logger.h"

//
// Monika data
//

SIZE_T MapSystemProviderRoutinesSize = sizeof(PS_PICO_PROVIDER_ROUTINES);
SIZE_T MapSystemPicoRoutinesSize = sizeof(PS_PICO_ROUTINES);
ULONG MapSystemAbiVersion = NTDDI_WIN10_RS4;
BOOLEAN MapSystemPicoHasSizeChecks = TRUE;
BOOLEAN MapTooLate = FALSE;

PS_PICO_PROVIDER_ROUTINES MapOriginalProviderRoutines;
PS_PICO_ROUTINES MapOriginalRoutines;

BOOLEAN MapPicoRegistrationDisabled = FALSE;
PS_PICO_PROVIDER_ROUTINES MapProviderRoutines[MaPicoProviderMaxCount];
PS_PICO_ROUTINES MapRoutines[MaPicoProviderMaxCount];
PS_PICO_ROUTINES MapRoutinesTh1[MaPicoProviderMaxCount];
MA_PICO_PROVIDER_ROUTINES MapAdditionalProviderRoutines[MaPicoProviderMaxCount];
MA_PICO_ROUTINES MapAdditionalRoutines[MaPicoProviderMaxCount];
SIZE_T MapProvidersCount = 0;

static LONG MaInitialized = FALSE;

static constinit UNICODE_STRING MaServiceRegistryKey = RTL_CONSTANT_STRING(
    L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\lxmonika"
);
static constinit UNICODE_STRING MaEnableLateRegistrationRegistryValue = RTL_CONSTANT_STRING(
    L"MaEnableLateRegistration"
);

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

    // Clear the initialized flag if any of the following code fails.
    PLONG pInitializedFlagGuard = &MaInitialized;
    AUTO_RESOURCE(pInitializedFlagGuard, [](auto p) { *p = FALSE; });

    MA_RETURN_IF_FAIL(PicoSppDetermineAbiStatus(
        &MapSystemProviderRoutinesSize,
        &MapSystemPicoRoutinesSize,
        &MapSystemAbiVersion,
        &MapSystemPicoHasSizeChecks,
        &MapTooLate
    ));

    if (!MapTooLate)
    {
        // We are loaded as a core driver and have the ability to register ourselves as a Pico
        // provider.

        MapOriginalProviderRoutines = PS_PICO_PROVIDER_ROUTINES
        {
            .Size = MapSystemProviderRoutinesSize,
            .DispatchSystemCall = MapSystemCallDispatch,
            .ExitThread = MapThreadExit,
            .ExitProcess = MapProcessExit,
            .DispatchException = MapDispatchException,
            .TerminateProcess = MapTerminateProcess,
            .WalkUserStack = MapWalkUserStack,
            .ProtectedRanges = NULL,
            .GetAllocatedProcessImageName = MapGetAllocatedProcessImageName,

            // Request full access. We will duplicate the handles before returning to consumers.
            .OpenProcess = PROCESS_ALL_ACCESS,
            .OpenThread = THREAD_ALL_ACCESS,

            // Not a good option, but there are no better ones.
            .SubsystemInformationType = (SUBSYSTEM_INFORMATION_TYPE)1
                // SubsystemInformationTypeWSL
        };

        MapOriginalRoutines = PS_PICO_ROUTINES
        {
            .Size = MapSystemPicoRoutinesSize

            // All other members set to 0 / NULL.
            // They will be left untouched if the system supports fewer routines.
        };

        if (MapSystemPicoHasSizeChecks)
        {
            MA_RETURN_IF_FAIL(PsRegisterPicoProvider(
                &MapOriginalProviderRoutines, &MapOriginalRoutines
            ));
        }
        else // No size check, skip the first Size field.
        {
            MA_RETURN_IF_FAIL(PsRegisterPicoProvider(
                (PPS_PICO_PROVIDER_ROUTINES)&MapOriginalProviderRoutines.DispatchSystemCall,
                (PPS_PICO_ROUTINES)&MapOriginalRoutines.CreateProcess
            ));
        }
    }
    else
    {
        Logger::LogWarning("lxmonika is loaded too late to register as a Pico provider.");

        OBJECT_ATTRIBUTES objectAttributes;
        InitializeObjectAttributes(
            &objectAttributes,
            &MaServiceRegistryKey,
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
            NULL,
            NULL
        );
        HANDLE hServicesRegistryKey = NULL;
        MA_RETURN_IF_FAIL(ZwOpenKey(
            &hServicesRegistryKey,
            KEY_READ,
            &objectAttributes
        ));

        union
        {
            KEY_VALUE_PARTIAL_INFORMATION keyInformation;
            CHAR keyBuffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(DWORD)];
        };
        ULONG ulResultLength = 0;

        NTSTATUS status = ZwQueryValueKey(
            hServicesRegistryKey,
            &MaEnableLateRegistrationRegistryValue,
            KeyValuePartialInformation,
            &keyInformation,
            sizeof(keyBuffer),
            &ulResultLength
        );

        if (!NT_SUCCESS(status)
            || keyInformation.Type != REG_DWORD
            || keyInformation.DataLength != sizeof(DWORD)
            || *((DWORD*)keyInformation.Data) == 0)
        {
            Logger::LogError("Refusing to use unsupported heuristics.");
            Logger::LogError(
                "To enable offset-based heuristics, set the \"",
                &MaEnableLateRegistrationRegistryValue, "\" registry value to 1."
            );
            Logger::LogError(
                "Alternatively, run \"monika.exe install\" again with the "
                "\"--enable-late-registration\" switch."
            );
            return STATUS_TOO_LATE;
        }
        else
        {
            Logger::LogWarning("Falling back to unsupported heuristics.");
            Logger::LogWarning("This may trigger PatchGuard and cause system instability.");
        }

        PPS_PICO_ROUTINES pRoutines = NULL;
        PPS_PICO_PROVIDER_ROUTINES pProviderRoutines = NULL;

        MA_RETURN_IF_FAIL(PicoSppLocateRoutines(&pRoutines));

        MapOriginalRoutines = *pRoutines;

        if (pRoutines->Size != sizeof(MapOriginalRoutines))
        {
            Logger::LogWarning("Expected size ", sizeof(MapOriginalRoutines),
                " for struct PS_PICO_ROUTINES, but got ", pRoutines->Size);
            Logger::LogWarning("Please update the pico struct definitions.");
        }

        MA_RETURN_IF_FAIL(PicoSppLocateProviderRoutines(&pProviderRoutines));

        MapOriginalProviderRoutines = *pProviderRoutines;

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
    }

    // Initialize pico routines
#define MONIKA_PROVIDER(index)                                                  \
    MapRoutines[MaPicoProvider##index] =                                        \
    {                                                                           \
        .Size = sizeof(PS_PICO_ROUTINES),                                       \
        .CreateProcess = MaPicoCreateProcess##index,                            \
        .CreateThread = MaPicoCreateThread##index,                              \
        .GetProcessContext = MaPicoGetProcessContext##index,                    \
        .GetThreadContext = MaPicoGetThreadContext##index,                      \
        .GetContextThreadInternal = MaPicoGetContextThreadInternal##index,      \
        .SetContextThreadInternal = MaPicoSetContextThreadInternal##index,      \
        .TerminateThread = MaPicoTerminateThread##index,                        \
        .ResumeThread = MaPicoResumeThread##index,                              \
        .SetThreadDescriptorBase = MaPicoSetThreadDescriptorBase##index,        \
        .SuspendThread = MaPicoSuspendThread##index,                            \
        .TerminateProcess = MaPicoTerminateProcess##index                       \
    };                                                                          \
                                                                                \
    MapRoutinesTh1[MaPicoProvider##index] = MapRoutines[MaPicoProvider##index]; \
    MapRoutinesTh1[MaPicoProvider##index].CreateProcess =                       \
        (PPS_PICO_CREATE_PROCESS)MaPicoCreateProcessTh1##index;                 \
    MapRoutinesTh1[MaPicoProvider##index].CreateThread =                        \
        (PPS_PICO_CREATE_THREAD)MaPicoCreateThreadTh1##index;
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

    // Everything is OK, release the guard.
    pInitializedFlagGuard = NULL;

    return STATUS_SUCCESS;
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
            if (!MapTooLate)
            {
                // Wire lxcore directly to the system.
                *pRoutines = MapProviderRoutines[MapLxssProviderIndex];
            }
            else
            {
                // The original ones are the ones belonging to lxcore.
                *pRoutines = MapOriginalProviderRoutines;
            }
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
    if (ProviderRoutines->Size > sizeof(PS_PICO_PROVIDER_ROUTINES)
        || PicoRoutines->Size > sizeof(PS_PICO_ROUTINES))
    {
        if (MmIsAddressValid((PVOID)ProviderRoutines->Size)
            || MmIsAddressValid((PVOID)PicoRoutines->Size))
        {
            // The driver was not aware about the Size member.
            // Therefore, those insanely big SIZE_T values are actually pointers.
            // This only happens in very old versions of lxcore.
            // (Or when some random noob messing up the structs).
            //
            // We do not know how much data to read, so just fail.
            Logger::LogTrace("The provider did not tell lxmonika the struct sizes.");
        }
        else
        {
            // This driver comes from an optimistic future where M$ worked on Pico providers again.
            // (Or from some random noob messing up the callback sizes).
            //
            // The Pico ABI it uses is newer than lxmonika and unlikely to be compatible.
            Logger::LogTrace("Detected a driver with more routines than lxmonika knows.");
            Logger::LogTrace("ProviderRoutines->Size: ", ProviderRoutines->Size);
            Logger::LogTrace("PicoRoutines->Size: ", PicoRoutines->Size);
        }

        return STATUS_INFO_LENGTH_MISMATCH;
    }

    DWORD dwAbiVersion = 0;
    if (AdditionalProviderRoutines != NULL
        && AdditionalProviderRoutines->Size >=
            (FIELD_OFFSET(MA_PICO_PROVIDER_ROUTINES, AbiVersion)
                + sizeof(MA_PICO_PROVIDER_ROUTINES::AbiVersion)))
    {
        // Respect any ABI claims by lxmonika-aware providers.
        dwAbiVersion = AdditionalProviderRoutines->AbiVersion;
        // Only if they are valid.
        //
        // Windows 8.1 is the first known version to have anything Pico-related.
        //
        // OSVER has not increased above 0x0A000000 since the first release of Windows NT 10.0.
        // If the reported value is greater than that, it's likely to be a pointer.
        // We will be a bit generous and go up to 0x0C000000, which rules out everything with
        // the most significant bit set (usually kernel mode pointers in 32-bit OSes).
        if (dwAbiVersion < NTDDI_WINBLUE || dwAbiVersion > 0x0C000000)
        {
            Logger::LogWarning("Provider is reporting an invalid ABI version: ",
                (PVOID)AdditionalProviderRoutines->AbiVersion);

            return STATUS_INVALID_PARAMETER;
        }
    }
    if (dwAbiVersion == 0)
    {
        // The additional routines were NULL or did not have an ABI version.
        MA_RETURN_IF_FAIL(PicoSppDetermineAbiVersion(
            ProviderRoutines,
            PicoRoutines,
            &dwAbiVersion
        ));
    }

    if (dwAbiVersion >= NTDDI_WIN10_RS1)
    {
        // RS1 Pico structs require these two ACCESS_MASK members.
        if ((ProviderRoutines->OpenProcess & (~PROCESS_ALL_ACCESS)) != 0
            || (ProviderRoutines->OpenThread & (~THREAD_ALL_ACCESS)) != 0)
        {
            return STATUS_INVALID_PARAMETER;
        }
    }

    if (MapPicoRegistrationDisabled)
    {
        // No, we are not evil like Microsoft.
        //
        // MapPicoRegistrationDisabled is only set early during initialization to prevent lxcore
        // from registering a second time. Other drivers should not even be loaded yet.
        //
        // The return status is a bit ironic, it should have been STATUS_TOO_EARLY or something.
        // However, this one is compatible with what the function in ntoskrnl returns.
        return STATUS_TOO_LATE;
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

    // Make sure all trailing members are filled with zero.
    memset(&MapProviderRoutines[uProviderIndex], 0, sizeof(PS_PICO_PROVIDER_ROUTINES));
    memcpy(&MapProviderRoutines[uProviderIndex], ProviderRoutines, ProviderRoutines->Size);

    // Keep the potentially larger size, hoping that some drivers
    // might know that they are outdated.
    if (dwAbiVersion >= NTDDI_WIN10_RS2)
    {
        // This is the ABI for RS4.
        memcpy(PicoRoutines, &MapRoutines[uProviderIndex], PicoRoutines->Size);
        // RS3 or lower still uses the CreateProcess callback from TH1.
        if (dwAbiVersion < NTDDI_WIN10_RS4)
        {
            if (PicoRoutines->Size >=
                (FIELD_OFFSET(PS_PICO_ROUTINES, CreateProcess)
                    + sizeof(PS_PICO_ROUTINES::CreateProcess)))
            {
                PicoRoutines->CreateProcess = MapRoutinesTh1[uProviderIndex].CreateProcess;
            }
        }
    }
    else // NTDDI_WIN10
    {
        memcpy(PicoRoutines, &MapRoutinesTh1[uProviderIndex], PicoRoutines->Size);
    }

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

    // Always set the correct detected ABI version.
    MapAdditionalProviderRoutines[uProviderIndex].AbiVersion = dwAbiVersion;

    if (Index != NULL)
    {
        *Index = uProviderIndex;
    }

    if (MapLxssRegistering)
    {
        // The hooked built-in function has no way to inform the index.
        MapLxssProviderIndex = uProviderIndex;
    }

    Logger::LogTrace("Registered Pico provider #", uProviderIndex, ".");
    Logger::LogTrace("Identified ABI version is ", (PVOID)(SIZE_T)dwAbiVersion, ".");

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
