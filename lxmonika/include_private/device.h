#pragma once

#include <ntifs.h>

// device.h
//
// Device registration and handling

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _DEVICE_SET DEVICE_SET, *PDEVICE_SET;

NTSTATUS
    DevpInit(
        _Inout_ PDRIVER_OBJECT pDriverObject
    );

// DevpRegisterDeviceSet
//
// Registers that the recently added set of devices share the same major functions.
NTSTATUS
    DevpRegisterDeviceSet(
        _Inout_ PDRIVER_OBJECT pDriverObject,
        _Out_opt_ PDEVICE_SET* pDeviceSet
    );

NTSTATUS
    DevpUnregisterDeviceSet(
        _In_ PDEVICE_SET pDeviceSet
    );

#ifdef __cplusplus
}
#endif
