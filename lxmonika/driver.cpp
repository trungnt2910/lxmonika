#include "driver.h"

#include <ntddk.h>

#include "Logger.h"

#include "module.h"
#include "monika.h"
#include "picosupport.h"

PDRIVER_OBJECT DriverGlobalObject;

extern "C"
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT     DriverObject,
    _In_ PUNICODE_STRING    RegistryPath
)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status = STATUS_SUCCESS;

    Logger::LogInfo("Hello World from lxmonika!");

    DriverGlobalObject = DriverObject;

    // According to Microsoft naming conventions:
    // Ma           => MonikA
    // p            => Private function
    // Initialize   => Function name
    // This function has nothing to do with maps.
    status = MapInitialize();

    DriverObject->DriverUnload = DriverUnload;

    return status;
}

extern "C"
VOID
DriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);

    MapCleanup();

    DriverGlobalObject = NULL;
}
