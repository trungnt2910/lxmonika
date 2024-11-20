#pragma once

#include <ntifs.h>
#include <ntintsafe.h>

#include "compat.h"
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

/// <summary>
/// Determines the Pico ABI status of the host system.
/// </summary>
NTSTATUS
    PicoSppDetermineAbiStatus(
        _Out_ PSIZE_T pProviderRoutinesSize,
        _Out_ PSIZE_T pPicoRoutinesSize,
        _Out_ DWORD* pAbiVersion,
        _Out_ PBOOLEAN pHasSizeChecks,
        _Out_ PBOOLEAN pTooLate
    );

/// <summary>
/// Determine the appropriate Pico ABI version given the passed structure sizes.
/// </summary>
NTSTATUS
    PicoSppDetermineAbiVersion(
        _In_ PPS_PICO_PROVIDER_ROUTINES pProviderRoutines,
        _In_ PPS_PICO_ROUTINES pPicoRoutines,
        _Out_ DWORD* pAbiVersion
    );

#ifdef __cplusplus
}
#endif
