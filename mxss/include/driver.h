#pragma once

#include <ntddk.h>

// driver.h
//
// Standard driver entry points.

#ifdef __cplusplus
extern "C"
{
#endif

NTSTATUS
    DriverEntry(
        _In_ PDRIVER_OBJECT     DriverObject,
        _In_ PUNICODE_STRING    RegistryPath
    );

VOID
    DriverUnload(
        _In_ PDRIVER_OBJECT DriverObject
    );

#ifdef __cplusplus
}
#endif
