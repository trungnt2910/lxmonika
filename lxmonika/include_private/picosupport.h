#pragma once

#include <ntifs.h>

#include "pico.h"
#include "picooffsets.h"

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

NTSTATUS
    PicoSppGetOffsets(
        _In_ PCSTR pVersion,
        _In_opt_ PCSTR pArchitecture,
        _Out_ PMA_PSP_PICO_PROVIDER_ROUTINES_OFFSETS* pPOffsets
    );

#ifdef __cplusplus
}
#endif
