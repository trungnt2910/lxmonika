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
        OUT PPS_PICO_PROVIDER_ROUTINES* pPpr
    );

#ifdef __cplusplus
}
#endif
