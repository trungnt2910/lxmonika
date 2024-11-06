#include "driver.h"

#include <ntddk.h>

#include "Logger.h"

#include "device.h"
#include "module.h"
#include "monika.h"
#include "picosupport.h"
#include "reality.h"

#include "AutoResource.h"

PDRIVER_OBJECT DriverGlobalObject;
LONG DriverInitialized = FALSE;

extern "C"
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT     DriverObject,
    _In_ PUNICODE_STRING    RegistryPath
)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status;

    // On systems where lxmonika "borrows" a core service then still registers itself to allow
    // other Pico providers to declare "lxmonika" as a dependency, this driver entry point may
    // be called twice: Once in early boot as a "Core" driver, the second time as a normal Boot
    // driver.
    // Make sure that all the initialization routines are only run once.
    if (InterlockedCompareExchange(&DriverInitialized, TRUE, FALSE) == FALSE)
    {
        Logger::LogInfo("Hello World from lxmonika!");
        Logger::LogInfo("lxmonika is running as \"", &DriverObject->DriverName, "\".");
    }
    else
    {
        Logger::LogTrace("Ignoring reload attempt as \"", &DriverObject->DriverName, "\".");
        DriverObject->DriverUnload = DriverUnload;
        return STATUS_SUCCESS;
    }

    // Clear the initialized flag if any of the following code fails.
    PLONG pDriverInitializedFlagGuard = &DriverInitialized;
    AUTO_RESOURCE(pDriverInitializedFlagGuard, [](auto p) { *p = FALSE; });

    MA_RETURN_IF_FAIL(DevpInit(DriverObject));

    // If we are "borrowing" a service from CNG, initialize it as soon as possible.
    constexpr UNICODE_STRING strDriverCng = RTL_CONSTANT_STRING(L"\\Driver\\CNG");
    if (RtlEqualUnicodeString(&DriverObject->DriverName, &strDriverCng, TRUE))
    {
        status = MapCngInitialize(DriverObject, RegistryPath);

        if (!NT_SUCCESS(status))
        {
            Logger::LogError("Failed to initialize CNG, status=", (PVOID)status);
            return status;
        }
    }

    // According to Microsoft naming conventions:
    // Ma           => MonikA
    // p            => Private function
    // Initialize   => Function name
    // This function has nothing to do with maps.
    status = MapInitialize();

    if (!NT_SUCCESS(status))
    {
        MapCngCleanup();

        Logger::LogError("Failed to initialize lxmonika, status=", (PVOID)status);
        return status;
    }

    // Optional step: Patching LXSS.
    status = MapLxssInitialize(DriverObject);

    if (!NT_SUCCESS(status))
    {
        Logger::LogWarning("Failed to integrate LXSS into lxmonika, status=", (PVOID)status);
        Logger::LogWarning("WSL integration features may not work.");
    }

    // /dev/reality and \Device\Reality
    // Initializing the reality devices requires knowing our DRIVER_OBJECT.
    // As MapInitialize may be indirectly called by other drivers through MaRegisterPicoProvider,
    // we cannot guarantee the availablity of DriverObject when MapInitialize is called.
    // For this reason, RlpInitializeDevices needs to be directly handled by DriverEntry.
    status = RlpInitializeDevices(DriverObject);

    // Should call this regardless of whether we succeeded.
    NTSTATUS statusUnpatch = MapLxssPrepareForPatchGuard();

    if (!NT_SUCCESS(statusUnpatch))
    {
        Logger::LogWarning("Problem undoing LXSS patches, status=", (PVOID)statusUnpatch);
        Logger::LogWarning("You may encounter issues with PatchGuard.");
    }

    if (!NT_SUCCESS(status))
    {
        MapLxssCleanup();
        MapCleanup();
        MapCngCleanup();

        // The reality device (at least the Win32 one) is required for userland hosts to launch
        // Pico processes.
        Logger::LogError("Failed to initialize the reality device, status=", (PVOID)status);
        return status;
    }

    // TODO: Disallow unloading altogether for the main driver object?
    // Especially when we're running as something like CNG.
    DriverObject->DriverUnload = DriverUnload;

    // Keep the object that we're attaching all our devices and stuff to.
    DriverGlobalObject = DriverObject;

    // The driver has successfully loaded, release the guard.
    pDriverInitializedFlagGuard = NULL;

    return STATUS_SUCCESS;
}

extern "C"
VOID
DriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    Logger::LogInfo("Unloading \"", &DriverObject->DriverName, "\".");

    if (DriverObject == DriverGlobalObject)
    {
        Logger::LogTrace("Cleaning up \"", &DriverObject->DriverName, "\".");

        RlpCleanupDevices();

        MapLxssCleanup();

        MapCleanup();

        MapCngCleanup();

        DriverGlobalObject = NULL;
        DriverInitialized = FALSE;
    }
    else
    {
        Logger::LogTrace(
            "No cleanup required for extra driver object \"", &DriverObject->DriverName, "\"."
        );
    }
}
