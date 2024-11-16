#pragma once

// compat.h
//
// Compatibility definitions.

//
// Enclave ID definitions
//

#ifndef ENCLAVE_SHORT_ID_LENGTH
#define ENCLAVE_SHORT_ID_LENGTH             16
#endif

#ifndef ENCLAVE_LONG_ID_LENGTH
#define ENCLAVE_LONG_ID_LENGTH              32
#endif

#if !defined(NTDDI_WIN10_VB) || (NTDDI_VERSION < NTDDI_WIN10_VB)
#define ExAllocatePool2         ExAllocatePoolWithTag
#endif

#if !defined(NTDDI_WIN10_RS2) || (NTDDI_VERSION < NTDDI_WIN10_RS2)
#define RtlGetNtSystemRoot()    (&SharedUserData->NtSystemRoot[0])
#endif

typedef struct _KADDRESS_RANGE_DESCRIPTOR
    KADDRESS_RANGE_DESCRIPTOR, *PKADDRESS_RANGE_DESCRIPTOR;

typedef enum _SUBSYSTEM_INFORMATION_TYPE SUBSYSTEM_INFORMATION_TYPE;
