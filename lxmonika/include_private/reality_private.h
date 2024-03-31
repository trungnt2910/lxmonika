#pragma once

// reality_private.h
//
// The reality device.

#include <ntifs.h>

#ifdef __cplusplus
extern "C"
{
#endif

//
// Reality lifetime functions
//

NTSTATUS
    RlpInitializeDevices(
        _In_ PDRIVER_OBJECT DriverObject
    );

VOID
    RlpCleanupDevices();

NTSTATUS
    RlpInitializeLxssDevice(
        _In_ PDRIVER_OBJECT DriverObject
    );

VOID
    RlpCleanupLxssDevice();

NTSTATUS
    RlpInitializeWin32Device(
        _In_ PDRIVER_OBJECT DriverObject
    );

VOID
    RlpCleanupWin32Device();

//
// Reality structs
//

typedef struct _RL_FILE {
    FAST_MUTEX  Lock;
    SIZE_T      Length;
    SIZE_T      Offset;
    CHAR        Data[2048];
} RL_FILE, *PRL_FILE;

//
// Reality files
//

NTSTATUS
    RlpFileOpen(
        _Out_ PRL_FILE pPFile
    );

NTSTATUS
    RlpFileRead(
        _Inout_ PRL_FILE pFile,
        _Out_writes_bytes_(szLength) PVOID pBuffer,
        _In_ SIZE_T szLength,
        _Inout_opt_ PINT64 pOffset,
        _Out_opt_ PSIZE_T pBytesTransferred
    );

NTSTATUS
    RlpFileWrite(
        _Inout_ PRL_FILE pFile,
        _In_reads_bytes_(szLength) PVOID pBuffer,
        _In_ SIZE_T szLength,
        _Inout_ PINT64 pOffset,
        _Out_opt_ PSIZE_T pBytesTransferred
    );

NTSTATUS
    RlpFileIoctl(
        _Inout_ PRL_FILE pFile,
        _In_ ULONG ulCode,
        _Inout_ PVOID pData
    );

NTSTATUS
    RlpFileSeek(
        _Inout_ PRL_FILE pFile,
        _In_ INT64 iOffset,
        _In_ INT iWhence,
        _Out_opt_ PINT64 pResultOffset
    );

//
// Reality defines
//

#define MA_REALITY_TAG ('lRaM')

#ifdef __cplusplus
}
#endif
