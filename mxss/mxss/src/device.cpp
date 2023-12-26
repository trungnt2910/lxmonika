#include "device.h"

#include <wdmsec.h>

#include "process.h"

#define IOCTL_MX_METHOD_BUFFERED \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _MX_EXECUTE_INFORMATION
{
    UNICODE_STRING ExecutablePath;
} MX_EXECUTE_INFORMATION, *PMX_EXECUTE_INFORMATION;

static DRIVER_DISPATCH MxControlDeviceNoOp;
static DRIVER_DISPATCH MxControlDeviceIoctl;

CONST UNICODE_STRING MxDeviceName = RTL_CONSTANT_STRING(L"\\Device\\mxss");
static PDEVICE_OBJECT MxDeviceObject = NULL;

extern "C"
NTSTATUS
DeviceInit(
    _In_ PDRIVER_OBJECT pDriverObject
)
{
    if (MxDeviceObject != NULL)
    {
        return STATUS_SUCCESS;
    }

    pDriverObject->MajorFunction[IRP_MJ_CREATE] = MxControlDeviceNoOp;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] = MxControlDeviceNoOp;
    pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MxControlDeviceIoctl;

    NTSTATUS status;

    status = IoCreateDeviceSecure(
        pDriverObject,
        0,
        (PUNICODE_STRING)&MxDeviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RW_RES_R,
        NULL,
        &MxDeviceObject
    );

    if (!NT_SUCCESS(status))
    {
        if (MxDeviceObject != NULL)
        {
            IoDeleteDevice(MxDeviceObject);
            MxDeviceObject = NULL;
        }
        return status;
    }

    return STATUS_SUCCESS;
}

extern "C"
VOID
DeviceCleanup(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);

    if (MxDeviceObject != NULL)
    {
        IoDeleteDevice(MxDeviceObject);
        MxDeviceObject = NULL;
    }
}

static
NTSTATUS
MxControlDeviceNoOp(
    _In_ PDEVICE_OBJECT pDeviceObject,
    _Inout_ PIRP pIrp
)
{
    UNREFERENCED_PARAMETER(pDeviceObject);
    pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = 0;

    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

static
NTSTATUS
MxControlDeviceIoctl(
    _In_ PDEVICE_OBJECT pDeviceObject,
    _Inout_ PIRP pIrp
)
{
    UNREFERENCED_PARAMETER(pDeviceObject);

    NTSTATUS status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = 0;

    PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);

    SIZE_T uInLen = pIrpStack->Parameters.DeviceIoControl.InputBufferLength;
    SIZE_T uOutLen = pIrpStack->Parameters.DeviceIoControl.OutputBufferLength;

    switch (pIrpStack->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_MX_METHOD_BUFFERED:
        {
            if (uInLen != sizeof(MX_EXECUTE_INFORMATION)
                || uOutLen != sizeof(NTSTATUS))
            {
                status = STATUS_INVALID_BUFFER_SIZE;
                break;
            }

            PMX_EXECUTE_INFORMATION pInfo = (PMX_EXECUTE_INFORMATION)
                pIrp->AssociatedIrp.SystemBuffer;
            PSTR pOutBuf = (PSTR)pIrp->AssociatedIrp.SystemBuffer;

            __try
            {
                ProbeForRead(pInfo->ExecutablePath.Buffer, pInfo->ExecutablePath.Length, 1);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                status = STATUS_ACCESS_VIOLATION;
                break;
            }

            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
                "Executing program: %ls\n", (PCWSTR)pInfo->ExecutablePath.Buffer);

            PMX_PROCESS pNewProcess;
            status = MxProcessExecute(
                &pInfo->ExecutablePath,
                PsGetCurrentProcess(),
                PsGetCurrentProcess(),
                &pNewProcess
            );

            if (!NT_SUCCESS(status))
            {
                break;
            }

            status = KeWaitForSingleObject(pNewProcess->Thread,
                Executive, KernelMode, FALSE, NULL);

            NTSTATUS statusExecute = pNewProcess->ExitStatus;
            MxProcessFree(pNewProcess);

            if (!NT_SUCCESS(status))
            {
                break;
            }

            memcpy(pOutBuf, &statusExecute, sizeof(NTSTATUS));

            pIrp->IoStatus.Information = sizeof(NTSTATUS);

            status = STATUS_SUCCESS;
        }
        break;
        default:
            DbgBreakPoint();
            status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    pIrp->IoStatus.Status = status;

    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return status;
}
