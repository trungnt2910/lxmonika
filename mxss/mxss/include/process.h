#pragma once

#include <ntifs.h>

// process.h
//
// Process support functions

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _MX_THREAD *PMX_THREAD;

typedef struct _MX_PROCESS {
    ULONG_PTR ReferenceCount;
    PEPROCESS Process;
    PETHREAD Thread;
    PMX_THREAD MxThread;
    PEPROCESS HostProcess;
    PFILE_OBJECT MainExecutable;
    NTSTATUS ExitStatus;
    PVOID UserStack;
} MX_PROCESS, *PMX_PROCESS;

NTSTATUS
    MxProcessExecute(
        _In_ PUNICODE_STRING pExecutablePath,
        _In_ PEPROCESS pParentProcess,
        _In_ PEPROCESS pHostProcess,
        _In_opt_ HANDLE hdlCwd,
        _Out_ PMX_PROCESS* pPMxProcess
    );

VOID
    MxProcessFree(
        _In_ PMX_PROCESS pMxProcess
    );

NTSTATUS
    MxProcessMapMainExecutable(
        _In_ PMX_PROCESS pMxProcess,
        _Out_ PVOID* pEntryPoint
    );

NTSTATUS
    MxProcessFork(
        _In_ PMX_PROCESS pMxParentProcess,
        _Out_ PMX_PROCESS* pPMxProcess
    );

#ifdef __cplusplus
}
#endif
