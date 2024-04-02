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
