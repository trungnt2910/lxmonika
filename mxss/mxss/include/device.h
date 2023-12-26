#pragma once

#include <ntifs.h>

// device.h
//
// mxss control device

#ifdef __cplusplus
extern "C"
{
#endif

NTSTATUS
    DeviceInit(
        _In_ PDRIVER_OBJECT pDriverObject
    );

VOID
    DeviceCleanup(
        _In_ PDRIVER_OBJECT pDriverObject
    );

#ifdef __cplusplus
}
#endif
