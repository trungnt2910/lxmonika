#pragma once

// reality.h
//
// The reality device.

#ifdef MONIKA_IN_DRIVER
#include "reality_private.h"
#endif

#define RL_DEVICE_CODE (0x13EA1)

#ifdef _WIN32
// TODO: Unify the device type with RL_DEVICE_CODE instead of using FILE_DEVICE_UNKNOWN.
#define RL_IOCTL_CODE(Function) \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800 | (Function), METHOD_BUFFERED, FILE_ANY_ACCESS)
#else
#define RL_IOCTL_CODE(Function) (Function)
#endif

enum RlIoctlFunctions
{
    RlIoctlPicoStartSession
};

#define RL_IOCTL_PICO_START_SESSION RL_IOCTL_CODE(RlIoctlPicoStartSession)

typedef struct _RL_PICO_SESSION_ATTRIBUTES {
    SIZE_T Size;
    // Understood as a index if value is smaller than MaPicoProviderMaxCount,
    // otherwise a pointer to the name.
    union {
        PUNICODE_STRING ProviderName;
        SIZE_T ProviderIndex;
    };
    PUNICODE_STRING RootDirectory;
    PUNICODE_STRING CurrentWorkingDirectory;
    SIZE_T ProviderArgsCount;
    PUNICODE_STRING ProviderArgs;
    SIZE_T ArgsCount;
    PUNICODE_STRING Args;
    SIZE_T EnvironmentCount;
    PUNICODE_STRING Environment;
} RL_PICO_SESSION_ATTRIBUTES, *PRL_PICO_SESSION_ATTRIBUTES;
