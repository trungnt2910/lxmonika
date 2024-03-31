#include "reality.h"

#include "Logger.h"

//
// Lifetime functions
//

extern "C"
NTSTATUS
RlpInitializeWin32Device(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);

    Logger::LogWarning("Win32 reality device is not implemented yet!");

    return STATUS_SUCCESS;
}

extern "C"
VOID
RlpCleanupWin32Device()
{
    // Currently no-op.
}
