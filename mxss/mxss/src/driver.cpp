#include "driver.h"

#include <monika.h>

#include "device.h"
#include "provider.h"

extern "C"
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT     DriverObject,
    _In_ PUNICODE_STRING    RegistryPath
)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status = STATUS_SUCCESS;

    status = DeviceInit(DriverObject);

    if (!NT_SUCCESS(status))
    {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "Failed to initialize control driver, status=%x\n", status));
        return status;
    }

    PS_PICO_PROVIDER_ROUTINES providerRoutines =
    {
        .Size = sizeof(PS_PICO_PROVIDER_ROUTINES),
        .DispatchSystemCall = MxSystemCallDispatch,
        .ExitThread = MxThreadExit,
        .ExitProcess = MxProcessExit,
        .DispatchException = MxDispatchException,
        .TerminateProcess = MxTerminateProcess,
        .WalkUserStack = MxWalkUserStack,
        .ProtectedRanges = NULL,
        .GetAllocatedProcessImageName = NULL,
        .OpenProcess = SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_CREATE_PROCESS |
            PROCESS_TERMINATE,
        .OpenThread = SYNCHRONIZE | THREAD_QUERY_LIMITED_INFORMATION | THREAD_TERMINATE,
        .SubsystemInformationType = SubsystemInformationTypeWSL
    };

    MxRoutines.Size = sizeof(PS_PICO_ROUTINES);

    status = MaRegisterPicoProvider(&providerRoutines, &MxRoutines);

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
        "Initialized Windows Subsystem for Monix, status=%x\n", status));

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = MaSetPicoProviderName(&providerRoutines, "Monix-0.0.1-alpha");

    if (!NT_SUCCESS(status))
    {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "Failed to set Pico provider name, status=%x\n", status));

        // Non-fatal.
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
    DeviceCleanup(DriverObject);

    // TODO: Unregister Pico provider when such an API exists.
}
