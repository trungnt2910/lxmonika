#pragma once

// picooffests.h
//
// Generated offsets for important symbols supporting Pico providers.

#include <ntddk.h>

typedef struct _MA_PSP_PICO_PROVIDER_ROUTINES_OFFSETS {
    PCSTR Version;
    PCSTR Architecture;
    struct {
        ULONG64 PspPicoRegistrationDisabled;
        ULONG64 PspPicoProviderRoutines;
        ULONG64 PspCreatePicoProcess;
        ULONG64 PspCreatePicoThread;
        ULONG64 PspGetPicoProcessContext;
        ULONG64 PspGetPicoThreadContext;
        ULONG64 PspPicoGetContextThreadEx;
        ULONG64 PspPicoSetContextThreadEx;
        ULONG64 PspTerminateThreadByPointer;
        ULONG64 PsResumeThread;
        ULONG64 PspSetPicoThreadDescriptorBase;
        ULONG64 PsSuspendThread;
        ULONG64 PspTerminatePicoProcess;
    } Offsets;
} MA_PSP_PICO_PROVIDER_ROUTINES_OFFSETS, *PMA_PSP_PICO_PROVIDER_ROUTINES_OFFSETS;

extern const MA_PSP_PICO_PROVIDER_ROUTINES_OFFSETS MaPspPicoProviderRoutinesOffsets[1325];

