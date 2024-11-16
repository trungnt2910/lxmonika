#include "reality.h"

#include <ntstrsafe.h>
#include <wdmsec.h>

#include "compat.h"

#include "AutoResource.h"
#include "Logger.h"

CONST UNICODE_STRING RlDeviceName = RTL_CONSTANT_STRING(RL_DEVICE_NAME);
static PDEVICE_OBJECT RlDeviceObject = NULL;

//
// Private function prototypes
//

static DRIVER_DISPATCH RlWin32DeviceCreate;
static DRIVER_DISPATCH RlWin32DeviceClose;
static DRIVER_DISPATCH RlWin32DeviceRead;
static DRIVER_DISPATCH RlWin32DeviceWrite;
static DRIVER_DISPATCH RlWin32DeviceControl;
static DRIVER_DISPATCH RlWin32DeviceSetInformation;

static PDRIVER_DISPATCH RlNextDeviceCreate          = NULL;
static PDRIVER_DISPATCH RlNextDeviceClose           = NULL;
static PDRIVER_DISPATCH RlNextDeviceRead            = NULL;
static PDRIVER_DISPATCH RlNextDeviceWrite           = NULL;
static PDRIVER_DISPATCH RlNextDeviceControl         = NULL;
static PDRIVER_DISPATCH RlNextDeviceSetInformation  = NULL;

//
// Lifetime functions
// Not thread-safe, but this should be fine since they are only called by DriverEntry/DriverUnload.
//

extern "C"
NTSTATUS
RlpInitializeWin32Device(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);

    if (RlDeviceObject != NULL)
    {
        return STATUS_SUCCESS;
    }

    // The DriverObject might already have major functions registered,
    // since we are sharing it with lxss.

    RlNextDeviceCreate = DriverObject->MajorFunction[IRP_MJ_CREATE];
    DriverObject->MajorFunction[IRP_MJ_CREATE] = RlWin32DeviceCreate;


    RlNextDeviceClose = DriverObject->MajorFunction[IRP_MJ_CLOSE];
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = RlWin32DeviceClose;

    RlNextDeviceRead = DriverObject->MajorFunction[IRP_MJ_READ];
    DriverObject->MajorFunction[IRP_MJ_READ] = RlWin32DeviceRead;

    RlNextDeviceWrite = DriverObject->MajorFunction[IRP_MJ_WRITE];
    DriverObject->MajorFunction[IRP_MJ_WRITE] = RlWin32DeviceWrite;

    // ioctl
    RlNextDeviceControl = DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = RlWin32DeviceControl;

    // seek
    RlNextDeviceSetInformation = DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION];
    DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] = RlWin32DeviceSetInformation;

    NTSTATUS status;

    status = IoCreateDeviceSecure(
        DriverObject,
        0,
        (PUNICODE_STRING)&RlDeviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RW_RES_R,
        NULL,
        &RlDeviceObject
    );

    if (!NT_SUCCESS(status))
    {
        if (RlDeviceObject != NULL)
        {
            IoDeleteDevice(RlDeviceObject);
            RlDeviceObject = NULL;
        }
        return status;
    }

    return STATUS_SUCCESS;
}

extern "C"
VOID
RlpCleanupWin32Device()
{
    if (RlDeviceObject != NULL)
    {
        IoDeleteDevice(RlDeviceObject);
        RlDeviceObject = NULL;
    }
}

//
// Driver dispatch functions
//

static
[[nodiscard]]
NTSTATUS
RlWin32CompleteRequest(
    _Inout_ PIRP pIrp,
    _In_ NTSTATUS status,
    _In_ ULONG_PTR uInfo = 0
)
{
    pIrp->IoStatus.Status = status;
    pIrp->IoStatus.Information = uInfo;

    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return status;
}

#define RL_TRY_DISPATCH_TO_NEXT(Function)                                                       \
    do                                                                                          \
    {                                                                                           \
        if (pDeviceObject != RlDeviceObject)                                                    \
        {                                                                                       \
            if (RlNextDevice##Function != NULL)                                                 \
            {                                                                                   \
                return RlNextDevice##Function(pDeviceObject, pIrp);                             \
            }                                                                                   \
            return STATUS_NOT_SUPPORTED;                                                        \
        }                                                                                       \
    }                                                                                           \
    while (0)

static
NTSTATUS
RlWin32DeviceCreate(
    _In_ PDEVICE_OBJECT pDeviceObject,
    _Inout_ PIRP pIrp
)
{
    Logger::LogTrace();

    RL_TRY_DISPATCH_TO_NEXT(Create);

    NTSTATUS status = STATUS_SUCCESS;

    PRL_FILE pNewFile = (PRL_FILE)ExAllocatePool2(PagedPool, sizeof(RL_FILE), MA_REALITY_TAG);
    if (pNewFile == NULL)
    {
        return RlWin32CompleteRequest(pIrp, STATUS_NO_MEMORY);
    }
    AUTO_RESOURCE(pNewFile, [](auto p) { ExFreePoolWithTag(p, MA_REALITY_TAG); });

    status = RlpFileOpen(pNewFile);

    if (!NT_SUCCESS(status))
    {
        return RlWin32CompleteRequest(pIrp, status);
    }

    PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
    pIrpStack->FileObject->FsContext = pNewFile;
    pNewFile = NULL;

    return RlWin32CompleteRequest(pIrp, STATUS_SUCCESS, FILE_OPENED);
}

static
NTSTATUS
RlWin32DeviceClose(
    _In_ PDEVICE_OBJECT pDeviceObject,
    _Inout_ PIRP pIrp
)
{
    Logger::LogTrace();

    RL_TRY_DISPATCH_TO_NEXT(Close);

    PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
    ExFreePoolWithTag(pIrpStack->FileObject->FsContext, MA_REALITY_TAG);

    return RlWin32CompleteRequest(pIrp, STATUS_SUCCESS);
}

static
NTSTATUS
RlWin32DeviceRead(
    _In_ PDEVICE_OBJECT pDeviceObject,
    _Inout_ PIRP pIrp
)
{
    Logger::LogTrace();

    RL_TRY_DISPATCH_TO_NEXT(Read);

    PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
    PRL_FILE pFile = (PRL_FILE)pIrpStack->FileObject->FsContext;

    INT64 iOffset = pIrpStack->Parameters.Read.ByteOffset.QuadPart;
    SIZE_T szBytesTransfered = 0;

    NTSTATUS status = RlpFileRead(
        pFile,
        pIrp->UserBuffer,
        pIrpStack->Parameters.Read.Length,
        &iOffset,
        &szBytesTransfered);

    return RlWin32CompleteRequest(pIrp, status, szBytesTransfered);
}

static
NTSTATUS
RlWin32DeviceWrite(
    _In_ PDEVICE_OBJECT pDeviceObject,
    _Inout_ PIRP pIrp
)
{
    Logger::LogTrace();

    RL_TRY_DISPATCH_TO_NEXT(Write);

    PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
    PRL_FILE pFile = (PRL_FILE)pIrpStack->FileObject->FsContext;

    INT64 iOffset;
    if (pIrpStack->Parameters.Write.ByteOffset.LowPart == FILE_WRITE_TO_END_OF_FILE)
    {
        iOffset = pFile->Length;
    }
    else
    {
        iOffset = pIrpStack->Parameters.Write.ByteOffset.QuadPart;
    }

    SIZE_T szBytesTransfered = 0;

    NTSTATUS status = RlpFileWrite(
        pFile,
        pIrp->UserBuffer,
        pIrpStack->Parameters.Write.Length,
        &iOffset,
        &szBytesTransfered);

    pIrp->IoStatus.Status = status;

    return RlWin32CompleteRequest(pIrp, status, szBytesTransfered);
}

static
NTSTATUS
RlWin32DeviceControl(
    _In_ PDEVICE_OBJECT pDeviceObject,
    _Inout_ PIRP pIrp
)
{
    Logger::LogTrace();

    RL_TRY_DISPATCH_TO_NEXT(Control);

    PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
    PRL_FILE pFile = (PRL_FILE)pIrpStack->FileObject->FsContext;

    // For reality device ioctls, the method is always METHOD_BUFFERED
    // (unlike read/write operations).
    //
    // Since we are providing a unified interface with the old LXSS reality device,
    // we want to be able to allow both the driver and the user code to share
    // a single buffer for input and output.

    NTSTATUS status = RlpFileIoctl(
        pFile,
        0x800 ^ IoGetFunctionCodeFromCtlCode(pIrpStack->Parameters.DeviceIoControl.IoControlCode),
        pIrp->AssociatedIrp.SystemBuffer
    );

    return RlWin32CompleteRequest(pIrp, status);
}

static
NTSTATUS
RlWin32DeviceSetInformation(
    _In_ PDEVICE_OBJECT pDeviceObject,
    _Inout_ PIRP pIrp
)
{
    Logger::LogTrace();

    RL_TRY_DISPATCH_TO_NEXT(SetInformation);

    PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
    PRL_FILE pFile = (PRL_FILE)pIrpStack->FileObject->FsContext;

    NTSTATUS status = STATUS_SUCCESS;

    switch (pIrpStack->Parameters.SetFile.FileInformationClass)
    {
    case FilePositionInformation:
    {
        PFILE_POSITION_INFORMATION pFilePositionInfo =
            (PFILE_POSITION_INFORMATION)pIrp->AssociatedIrp.SystemBuffer;
        status = RlpFileSeek(pFile, pFilePositionInfo->CurrentByteOffset.QuadPart, SEEK_SET, NULL);
        if (!NT_SUCCESS(status))
        {
            return RlWin32CompleteRequest(pIrp, status);
        }

        // "The Information member receives the number of bytes set on the file."
        return RlWin32CompleteRequest(pIrp, STATUS_SUCCESS, sizeof(FILE_POSITION_INFORMATION));
    }
    break;
    default:
    {
        return RlWin32CompleteRequest(pIrp, STATUS_NOT_SUPPORTED);
    }
    }
}
