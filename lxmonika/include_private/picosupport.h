#pragma once

#include <ntddk.h>

#include "pico.h"

// picosupport.h
//
// Support functions for Pico processes

#ifdef __cplusplus
extern "C"
{
#endif

NTSTATUS
    PicoSppLocateProviderRoutines(
        _Out_ PPS_PICO_PROVIDER_ROUTINES* pPpr
    );

NTSTATUS
    PicoSppLocateRoutines(
        _Out_ PPS_PICO_ROUTINES* pPr
    );

#ifdef __cplusplus
}
#endif
