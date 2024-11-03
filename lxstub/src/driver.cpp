#include <ntifs.h>

#include "compat.h"

#include "lxss.h"
#include "pico.h"

extern "C"
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT     DriverObject,
    _In_ PUNICODE_STRING    RegistryPath
)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
LxInitialize(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PLX_SUBSYSTEM Subsystem
)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(Subsystem);

    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_ERROR_LEVEL,
        "THIS IS A STUB! IT SHOULD ONLY BE USED IN THE BUILD SYSTEM!\n"
    );
    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_ERROR_LEVEL,
        "Please install the appropriate LXCORE.SYS from LXSS or AoW.\n"
    );
    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_ERROR_LEVEL,
        MONIKA_BUILD_NUMBER "\n"
    );

    // Make sure that we import this function.
    if ((PVOID)PsRegisterPicoProvider != (PVOID)DriverObject)
    {
        return STATUS_SUCCESS;
    }

    return STATUS_NOT_SUPPORTED;
}
