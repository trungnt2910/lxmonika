#pragma once

// compat.h
//
// Compatibility definitions.

#ifndef NTDDI_WIN7
#define NTDDI_WIN7                          0x06010000
#endif

#ifndef NTDDI_WIN8
#define NTDDI_WIN8                          0x06020000
#endif

#ifndef NTDDI_WINBLUE
#define NTDDI_WINBLUE                       0x06030000
#endif

#ifndef NTDDI_WINTHRESHOLD
#define NTDDI_WINTHRESHOLD                  0x0A000000
#endif

#ifndef NTDDI_WIN10
#define NTDDI_WIN10                         0x0A000000
#endif

#ifndef NTDDI_WIN10_TH2
#define NTDDI_WIN10_TH2                     0x0A000001
#endif

#ifndef NTDDI_WIN10_RS1
#define NTDDI_WIN10_RS1                     0x0A000002
#endif

#ifndef NTDDI_WIN10_RS2
#define NTDDI_WIN10_RS2                     0x0A000003
#endif

#ifndef NTDDI_WIN10_RS3
#define NTDDI_WIN10_RS3                     0x0A000004
#endif

#ifndef NTDDI_WIN10_RS4
#define NTDDI_WIN10_RS4                     0x0A000005
#endif

#ifndef NTDDI_WIN10_RS5
#define NTDDI_WIN10_RS5                     0x0A000006
#endif

#ifndef NTDDI_WIN10_19H1
#define NTDDI_WIN10_19H1                    0x0A000007
#endif

#ifndef NTDDI_WIN10_VB
#define NTDDI_WIN10_VB                      0x0A000008
#endif

#ifndef NTDDI_WIN10_MN
#define NTDDI_WIN10_MN                      0x0A000009
#endif

#ifndef NTDDI_WIN10_FE
#define NTDDI_WIN10_FE                      0x0A00000A
#endif

#ifndef NTDDI_WIN10_CO
#define NTDDI_WIN10_CO                      0x0A00000B
#endif

#ifndef NTDDI_WIN10_NI
#define NTDDI_WIN10_NI                      0x0A00000C
#endif

//
// Enclave ID definitions
//

#ifndef ENCLAVE_SHORT_ID_LENGTH
#define ENCLAVE_SHORT_ID_LENGTH             16
#endif

#ifndef ENCLAVE_LONG_ID_LENGTH
#define ENCLAVE_LONG_ID_LENGTH              32
#endif

#if (NTDDI_VERSION < NTDDI_WIN10_VB)
#define ExAllocatePool2         ExAllocatePoolWithTag
#endif

#if (NTDDI_VERSION < NTDDI_WIN10_RS2)
#define RtlGetNtSystemRoot()    (&SharedUserData->NtSystemRoot[0])
#endif

typedef struct _KADDRESS_RANGE_DESCRIPTOR
    KADDRESS_RANGE_DESCRIPTOR, *PKADDRESS_RANGE_DESCRIPTOR;

typedef enum _SUBSYSTEM_INFORMATION_TYPE SUBSYSTEM_INFORMATION_TYPE;
