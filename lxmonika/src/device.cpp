#include "device.h"

#include "monika.h"

#include "AutoResource.h"

//
// Helper macros
//

#define DEV_LIST_MAJOR_FUNCTIONS()                                                              \
    DEV_FOR_EACH_MAJOR_FUNCTION(CREATE)                                                         \
    DEV_FOR_EACH_MAJOR_FUNCTION(CREATE_NAMED_PIPE)                                              \
    DEV_FOR_EACH_MAJOR_FUNCTION(CLOSE)                                                          \
    DEV_FOR_EACH_MAJOR_FUNCTION(READ)                                                           \
    DEV_FOR_EACH_MAJOR_FUNCTION(WRITE)                                                          \
    DEV_FOR_EACH_MAJOR_FUNCTION(QUERY_INFORMATION)                                              \
    DEV_FOR_EACH_MAJOR_FUNCTION(SET_INFORMATION)                                                \
    DEV_FOR_EACH_MAJOR_FUNCTION(QUERY_EA)                                                       \
    DEV_FOR_EACH_MAJOR_FUNCTION(SET_EA)                                                         \
    DEV_FOR_EACH_MAJOR_FUNCTION(FLUSH_BUFFERS)                                                  \
    DEV_FOR_EACH_MAJOR_FUNCTION(QUERY_VOLUME_INFORMATION)                                       \
    DEV_FOR_EACH_MAJOR_FUNCTION(SET_VOLUME_INFORMATION)                                         \
    DEV_FOR_EACH_MAJOR_FUNCTION(DIRECTORY_CONTROL)                                              \
    DEV_FOR_EACH_MAJOR_FUNCTION(FILE_SYSTEM_CONTROL)                                            \
    DEV_FOR_EACH_MAJOR_FUNCTION(DEVICE_CONTROL)                                                 \
    DEV_FOR_EACH_MAJOR_FUNCTION(INTERNAL_DEVICE_CONTROL)                                        \
    DEV_FOR_EACH_MAJOR_FUNCTION(SHUTDOWN)                                                       \
    DEV_FOR_EACH_MAJOR_FUNCTION(LOCK_CONTROL)                                                   \
    DEV_FOR_EACH_MAJOR_FUNCTION(CLEANUP)                                                        \
    DEV_FOR_EACH_MAJOR_FUNCTION(CREATE_MAILSLOT)                                                \
    DEV_FOR_EACH_MAJOR_FUNCTION(QUERY_SECURITY)                                                 \
    DEV_FOR_EACH_MAJOR_FUNCTION(SET_SECURITY)                                                   \
    DEV_FOR_EACH_MAJOR_FUNCTION(POWER)                                                          \
    DEV_FOR_EACH_MAJOR_FUNCTION(SYSTEM_CONTROL)                                                 \
    DEV_FOR_EACH_MAJOR_FUNCTION(DEVICE_CHANGE)                                                  \
    DEV_FOR_EACH_MAJOR_FUNCTION(QUERY_QUOTA)                                                    \
    DEV_FOR_EACH_MAJOR_FUNCTION(SET_QUOTA)                                                      \
    DEV_FOR_EACH_MAJOR_FUNCTION(PNP)

#define DEV_POOL_TAG ('vDaM')

//
// Structures
//

typedef struct _DEVICE_ENTRY {
    LIST_ENTRY ListEntry;
    PDEVICE_OBJECT DeviceObject;
} DEVICE_ENTRY, *PDEVICE_ENTRY;

typedef struct _DEVICE_SET {
    LIST_ENTRY ListEntry;
    DEVICE_ENTRY DeviceList;
    PDRIVER_DISPATCH MajorFunctions[IRP_MJ_MAXIMUM_FUNCTION + 1];
} DEVICE_SET, *PDEVICE_SET;

#define DEV_CONTAINING_ENTRY(address)   \
    CONTAINING_RECORD(address, DEVICE_ENTRY, ListEntry)

#define DEV_CONTAINING_SET(address)     \
    CONTAINING_RECORD(address, DEVICE_SET, ListEntry)

//
// Data
//

static FAST_MUTEX DevLock;

static PDRIVER_DISPATCH DevBlankMajorFunctions[IRP_MJ_MAXIMUM_FUNCTION + 1];
static PDRIVER_DISPATCH DevMajorFunctions[IRP_MJ_MAXIMUM_FUNCTION + 1];
static DEVICE_SET DevDeviceSetList;

//
// Implementation
//

#define DEV_FOR_EACH_MAJOR_FUNCTION(FUNC)  \
    static DRIVER_DISPATCH Devp_##FUNC;
DEV_LIST_MAJOR_FUNCTIONS()
#undef DEV_FOR_EACH_MAJOR_FUNCTION

NTSTATUS
DevpInit(
    _Inout_ PDRIVER_OBJECT pDriverObject
)
{
    memcpy(&DevBlankMajorFunctions, pDriverObject->MajorFunction, sizeof(DevBlankMajorFunctions));

#define DEV_FOR_EACH_MAJOR_FUNCTION(FUNC)  \
    DevMajorFunctions[IRP_MJ_##FUNC] = Devp_##FUNC;
    DEV_LIST_MAJOR_FUNCTIONS()
#undef DEV_FOR_EACH_MAJOR_FUNCTION

    memcpy(&pDriverObject->MajorFunction, &DevMajorFunctions, sizeof(DevMajorFunctions));

    InitializeListHead(&DevDeviceSetList.ListEntry);

    ExInitializeFastMutex(&DevLock);

    return STATUS_SUCCESS;
}

static
NTSTATUS
DevpFindDeviceSetUnlocked(
    _In_ PDEVICE_OBJECT pDeviceObject,
    _Out_ PDEVICE_SET* pContainingSet
)
{
    for (PLIST_ENTRY pI = DevDeviceSetList.ListEntry.Flink;
        pI != &DevDeviceSetList.ListEntry;
        pI = pI->Flink)
    {
        PDEVICE_SET pCurrentSet = DEV_CONTAINING_SET(pI);

        for (PLIST_ENTRY pJ = pCurrentSet->DeviceList.ListEntry.Flink;
            pJ != &pCurrentSet->DeviceList.ListEntry;
            pJ = pJ->Flink)
        {
            PDEVICE_ENTRY pCurrentEntry = DEV_CONTAINING_ENTRY(pJ);

            if (pCurrentEntry->DeviceObject == pDeviceObject)
            {
                *pContainingSet = pCurrentSet;
                return STATUS_SUCCESS;
            }
        }
    }

    return STATUS_NOT_FOUND;
}

static
NTSTATUS
DevpInsertDevice(
    _In_ PDEVICE_OBJECT pDeviceObject,
    _Inout_opt_ PDEVICE_SET* pCurrentSet
)
{
    // Check if the device has already been inserted.
    PDEVICE_SET pExistingSet = NULL;
    NTSTATUS status = DevpFindDeviceSetUnlocked(pDeviceObject, &pExistingSet);

    if (NT_SUCCESS(status) || status != STATUS_NOT_FOUND)
    {
        return status;
    }

    PDEVICE_SET pNewSet = NULL;
    AUTO_RESOURCE(pNewSet, [](auto p) { ExFreePoolWithTag(p, DEV_POOL_TAG); });

    PDEVICE_SET pSet = (pCurrentSet != NULL) ? *pCurrentSet : NULL;

    if (pSet == NULL)
    {
        // No preferred set yet, allocate a new one.
        pNewSet = (PDEVICE_SET)ExAllocatePool2(PagedPool, sizeof(DEVICE_SET), DEV_POOL_TAG);
        if (pNewSet == NULL)
        {
            return STATUS_NO_MEMORY;
        }

        InitializeListHead(&pNewSet->DeviceList.ListEntry);

        // Don't write to the output parameter yet, wait until we succeed.
        pSet = pNewSet;
    }

    PDEVICE_ENTRY pNewEntry = (PDEVICE_ENTRY)
        ExAllocatePool2(PagedPool, sizeof(DEVICE_ENTRY), DEV_POOL_TAG);
    if (pNewEntry == NULL)
    {
        return STATUS_NO_MEMORY;
    }
    AUTO_RESOURCE(pNewEntry, [](auto p) { ExFreePoolWithTag(p, DEV_POOL_TAG); });

    pNewEntry->DeviceObject = pDeviceObject;
    InsertTailList(&pSet->DeviceList.ListEntry, &pNewEntry->ListEntry);

    // Optional logging
    ULONG ulDeviceNameLength = 0;
    ObQueryNameString(pDeviceObject, NULL, 0, &ulDeviceNameLength);
    POBJECT_NAME_INFORMATION pDeviceName =
        (POBJECT_NAME_INFORMATION)ExAllocatePool2(PagedPool, ulDeviceNameLength, DEV_POOL_TAG);
    if (pDeviceName != NULL)
    {
        AUTO_RESOURCE(pDeviceName, [](auto p) { ExFreePoolWithTag(p, DEV_POOL_TAG); });
        if (NT_SUCCESS(ObQueryNameString(
            pDeviceObject,
            pDeviceName,
            ulDeviceNameLength,
            &ulDeviceNameLength
        )))
        {
            Logger::LogTrace(
                "Registered new device \"", &pDeviceName->Name,
                "\" to set ", pSet, "."
            );
        }
    }

    // We are succeeding. Release the auto resources.
    pNewEntry = NULL;
    if (pNewSet != NULL)
    {
        Logger::LogTrace("Registered new device set: ", pNewSet, ".");

        // Only do this if we actually did allocate a new set.
        InsertTailList(&DevDeviceSetList.ListEntry, &pNewSet->ListEntry);
        if (pCurrentSet != NULL)
        {
            *pCurrentSet = pNewSet;
        }
        pNewSet = NULL;
    }

    return STATUS_SUCCESS;
}

static
NTSTATUS
DevpFindMajorFunction(
    _In_ PDEVICE_OBJECT pDeviceObject,
    _In_ SIZE_T szMajorFunctionIndex,
    _Out_ PDRIVER_DISPATCH* pMajorFunction
)
{
    if (pDeviceObject == NULL
        || szMajorFunctionIndex > IRP_MJ_MAXIMUM_FUNCTION
        || pMajorFunction == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PFAST_MUTEX pLock = &DevLock;
    ExAcquireFastMutex(pLock);
    AUTO_RESOURCE(pLock, ExReleaseFastMutex);

    PDEVICE_SET pContainingSet = NULL;
    NTSTATUS status = DevpFindDeviceSetUnlocked(pDeviceObject, &pContainingSet);

    if (!NT_SUCCESS(status))
    {
        *pMajorFunction = DevBlankMajorFunctions[szMajorFunctionIndex];
    }
    else
    {
        *pMajorFunction = pContainingSet->MajorFunctions[szMajorFunctionIndex];
    }

    return STATUS_SUCCESS;
}

NTSTATUS
DevpRegisterDeviceSet(
    _Inout_ PDRIVER_OBJECT pDriverObject,
    _Out_opt_ PDEVICE_SET* pDeviceSet
)
{
    if (pDriverObject == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (pDriverObject->DeviceObject == NULL)
    {
        if (pDeviceSet != NULL)
        {
            *pDeviceSet = NULL;
        }
        return STATUS_SUCCESS;
    }

    PFAST_MUTEX pLock = &DevLock;
    ExAcquireFastMutex(pLock);
    AUTO_RESOURCE(pLock, ExReleaseFastMutex);

    PDEVICE_SET pCurrentSet = NULL;

    for (PDEVICE_OBJECT pDeviceObject = pDriverObject->DeviceObject;
        pDeviceObject != NULL;
        pDeviceObject = pDeviceObject->NextDevice)
    {
        MA_RETURN_IF_FAIL(DevpInsertDevice(pDeviceObject, &pCurrentSet));
    }

    if (pDeviceSet != NULL)
    {
        *pDeviceSet = pCurrentSet;
    }

    if (pCurrentSet != NULL)
    {
        // We have detected and inserted at least one new device.
        // Store the current set of major functions.
        for (SIZE_T i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i)
        {
            pCurrentSet->MajorFunctions[i] = pDriverObject->MajorFunction[i];

            // The driver has not modified this specific callback.
            // Probably it does not support this request type.
            if (pCurrentSet->MajorFunctions[i] == DevMajorFunctions[i])
            {
                pCurrentSet->MajorFunctions[i] = DevBlankMajorFunctions[i];
            }
        }
    }

    // Restore the managed major functions.
    memcpy(&pDriverObject->MajorFunction, &DevMajorFunctions, sizeof(DevMajorFunctions));

    return STATUS_SUCCESS;
}

NTSTATUS
DevpUnregisterDeviceSet(
    _In_ PDEVICE_SET pDeviceSet
)
{
    PFAST_MUTEX pLock = &DevLock;
    ExAcquireFastMutex(pLock);
    AUTO_RESOURCE(pLock, ExReleaseFastMutex);

    for (PLIST_ENTRY pEntry = DevDeviceSetList.ListEntry.Flink;
        pEntry != &DevDeviceSetList.ListEntry;
        pEntry = pEntry->Flink)
    {
        PDEVICE_SET pCurrentSet = DEV_CONTAINING_SET(pEntry);

        if (pCurrentSet == pDeviceSet)
        {
            PLIST_ENTRY pSetHead = &pCurrentSet->DeviceList.ListEntry;

            while (!IsListEmpty(pSetHead))
            {
                ExFreePoolWithTag(RemoveHeadList(pSetHead), DEV_POOL_TAG);
            }

            ExFreePoolWithTag(pDeviceSet, DEV_POOL_TAG);

            return STATUS_SUCCESS;
        }
    }

    return STATUS_NOT_FOUND;
}

#define DEV_FOR_EACH_MAJOR_FUNCTION(FUNC)                                                       \
    static NTSTATUS Devp_##FUNC(                                                                \
        _In_ PDEVICE_OBJECT DeviceObject,                                                       \
        _Inout_ PIRP Irp                                                                        \
    )                                                                                           \
    {                                                                                           \
            PDRIVER_DISPATCH pDispatch = NULL;                                                  \
            MA_RETURN_IF_FAIL(DevpFindMajorFunction(                                            \
                DeviceObject,                                                                   \
                IRP_MJ_##FUNC,                                                                  \
                &pDispatch                                                                      \
            ));                                                                                 \
            return pDispatch(DeviceObject, Irp);                                                \
    }
DEV_LIST_MAJOR_FUNCTIONS()
#undef DEV_FOR_EACH_MAJOR_FUNCTION
