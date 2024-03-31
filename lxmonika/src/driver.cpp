#include "driver.h"

#include <ntddk.h>

#include "Logger.h"

#include "module.h"
#include "monika.h"
#include "picosupport.h"
#include "reality.h"

PDRIVER_OBJECT DriverGlobalObject;

extern "C"
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT     DriverObject,
    _In_ PUNICODE_STRING    RegistryPath
)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status;

    Logger::LogInfo("Hello World from lxmonika!");

    // According to Microsoft naming conventions:
    // Ma           => MonikA
    // p            => Private function
    // Initialize   => Function name
    // This function has nothing to do with maps.
    status = MapInitialize();

    if (!NT_SUCCESS(status))
    {
        Logger::LogError("Failed to initialize lxmonika, status=", (PVOID)status);
        return status;
    }

    // /dev/reality and \Device\Reality
    // Initializing the reality devices requires knowing our DRIVER_OBJECT.
    // As MapInitialize may be indirectly called by other drivers through MaRegisterPicoProvider,
    // we cannot guarantee the availablity of DriverObject when MapInitialize is called.
    // For this reason, RlpInitializeDevices needs to be directly handled by DriverEntry.
    status = RlpInitializeDevices(DriverObject);

    if (!NT_SUCCESS(status))
    {
        MapCleanup();

        // The reality device (at least the Win32 one) is required for userland hosts to launch
        // Pico processes.
        Logger::LogError("Failed to initialize the reality device, status=", (PVOID)status);
        return status;
    }

    DriverObject->DriverUnload = DriverUnload;

    return STATUS_SUCCESS;
}

extern "C"
VOID
DriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);

    RlpCleanupDevices();

    MapCleanup();
}
