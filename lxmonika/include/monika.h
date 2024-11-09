#pragma once

// monika.h
//
// Just Monika.

#include <ntifs.h>

#include "pico.h"

#ifdef MONIKA_IN_DRIVER
#define MONIKA_EXPORT __declspec(dllexport)
#else
#define MONIKA_EXPORT __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef
NTSTATUS
    MA_PICO_GET_ALLOCATED_PROVIDER_NAME(
        _Outptr_ PUNICODE_STRING* ProviderName
    );
typedef MA_PICO_GET_ALLOCATED_PROVIDER_NAME* PMA_PICO_GET_ALLOCATED_PROVIDER_NAME;

typedef struct _MA_PICO_SESSION_ATTRIBUTES {
    SIZE_T Size;
    HANDLE HostProcess;
    HANDLE Console;
    HANDLE Input;
    HANDLE Output;
    HANDLE RootDirectory;
    HANDLE CurrentWorkingDirectory;
    SIZE_T ProviderArgsCount;
    PUNICODE_STRING ProviderArgs;
    SIZE_T ArgsCount;
    PUNICODE_STRING Args;
    SIZE_T EnvironmentCount;
    PUNICODE_STRING Environment;
} MA_PICO_SESSION_ATTRIBUTES, *PMA_PICO_SESSION_ATTRIBUTES;

typedef
NTSTATUS
    MA_PICO_START_SESSION(
        _In_ PMA_PICO_SESSION_ATTRIBUTES Attributes
    );
typedef MA_PICO_START_SESSION* PMA_PICO_START_SESSION;

typedef
NTSTATUS
    MA_PICO_GET_CURRENT_WORKING_DIRECTORY(
        _In_ PEPROCESS Process,
        _Out_ PHANDLE CurrentWorkingDirectory
    );
typedef MA_PICO_GET_CURRENT_WORKING_DIRECTORY* PMA_PICO_GET_CURRENT_WORKING_DIRECTORY;

typedef
NTSTATUS
    MA_PICO_GET_CONSOLE(
        _In_ PEPROCESS Process,
        _Out_opt_ PHANDLE Console,
        _Out_opt_ PHANDLE Input,
        _Out_opt_ PHANDLE Output
    );
typedef MA_PICO_GET_CONSOLE* PMA_PICO_GET_CONSOLE;

typedef struct _MA_PICO_PROVIDER_ROUTINES {
    SIZE_T Size;
    PMA_PICO_GET_ALLOCATED_PROVIDER_NAME GetAllocatedProviderName;
    PMA_PICO_START_SESSION StartSession;
    PMA_PICO_GET_CURRENT_WORKING_DIRECTORY GetCurrentWorkingDirectory;
    PMA_PICO_GET_CONSOLE GetConsole;
} MA_PICO_PROVIDER_ROUTINES, *PMA_PICO_PROVIDER_ROUTINES;

typedef struct _MA_PICO_ROUTINES {
    SIZE_T Size;
} MA_PICO_ROUTINES;
typedef MA_PICO_ROUTINES* PMA_PICO_ROUTINES;

MONIKA_EXPORT
NTSTATUS NTAPI
    MaRegisterPicoProvider(
        _In_ PPS_PICO_PROVIDER_ROUTINES ProviderRoutines,
        _Inout_ PPS_PICO_ROUTINES PicoRoutines
    );

MONIKA_EXPORT
NTSTATUS NTAPI
    MaRegisterPicoProviderEx(
        _In_ PPS_PICO_PROVIDER_ROUTINES ProviderRoutines,
        _Inout_ PPS_PICO_ROUTINES PicoRoutines,
        _In_opt_ PMA_PICO_PROVIDER_ROUTINES AdditionalProviderRoutines,
        _Inout_opt_ PMA_PICO_ROUTINES AdditionalPicoRoutines,
        _Out_opt_ PSIZE_T Index
    );

/// <summary>Sets the name of the specified provider.</summary>
///
/// <param name="Name">
/// A null-terminated Unicode string. The name is recommended (but not required) to have the
/// following format: <c>"{Kernel Name}-{Kernel Release}-{Other details}"</c>.
/// </param>
///
/// <param name="Index">
/// A pointer to receive the found index.
/// </param>
///
/// <remarks>
/// The function looks for providers based on the names provided by the
/// <c>GetAllocatedProviderName</c> extended provider routine.
/// The first provider reporting the name with the longest common prefix with the queried name will
/// be selected.
/// For example, if "Linux-5.15" and "Linux-6.6" are reported in this order, "Linux" would match
/// "Linux-5.15", while "Linux-6" would match "Linux-6.6".
/// </remarks>
MONIKA_EXPORT
NTSTATUS NTAPI
    MaFindPicoProvider(
        _In_ PCWSTR ProviderName,
        _Out_ PSIZE_T Index
    );

MONIKA_EXPORT
NTSTATUS NTAPI
    MaGetAllocatedPicoProviderName(
        _In_ SIZE_T Index,
        _Out_ PUNICODE_STRING* ProviderName
    );

MONIKA_EXPORT
NTSTATUS NTAPI
    MaStartSession(
        _In_ SIZE_T Index,
        _In_ PMA_PICO_SESSION_ATTRIBUTES SessionAttributes
    );

MONIKA_EXPORT
NTSTATUS NTAPI
    MaGetConsole(
        _In_ PEPROCESS Process,
        _Out_opt_ PHANDLE Console,
        _Out_opt_ PHANDLE Input,
        _Out_opt_ PHANDLE Output
    );

//
// Monika utilities
//

MONIKA_EXPORT
NTSTATUS NTAPI
    MaUtilDuplicateKernelHandle(
        _In_ HANDLE SourceHandle,
        _Out_ PHANDLE TargetHandle
    );

#include "monika_constants.h"

#ifdef __cplusplus
}
#endif

#ifdef MONIKA_IN_DRIVER
#include "monika_private.h"
#endif
