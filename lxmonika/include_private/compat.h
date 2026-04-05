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

//
// ExAllocatePool2
//

#if (NTDDI_VERSION < NTDDI_WIN10_VB)

//
// POOL_FLAG values
//
// Low 32-bits of ULONG64 are for required parameters (allocation fails if they
// cannot be satisfied).
// High 32-bits of ULONG64 is for optional parameters (allocation succeeds if
// they cannot be satisfied or are unrecognized).
//

#define POOL_FLAG_REQUIRED_START          0x0000000000000001UI64
#define POOL_FLAG_USE_QUOTA               0x0000000000000001UI64     // Charge quota
#define POOL_FLAG_UNINITIALIZED           0x0000000000000002UI64     // Don't zero-initialize allocation
#define POOL_FLAG_SESSION                 0x0000000000000004UI64     // Use session specific pool
#define POOL_FLAG_CACHE_ALIGNED           0x0000000000000008UI64     // Cache aligned allocation
#define POOL_FLAG_RESERVED1               0x0000000000000010UI64     // Reserved for system use
#define POOL_FLAG_RAISE_ON_FAILURE        0x0000000000000020UI64     // Raise exception on failure
#define POOL_FLAG_NON_PAGED               0x0000000000000040UI64     // Non paged pool NX
#define POOL_FLAG_NON_PAGED_EXECUTE       0x0000000000000080UI64     // Non paged pool executable
#define POOL_FLAG_PAGED                   0x0000000000000100UI64     // Paged pool
#define POOL_FLAG_RESERVED2               0x0000000000000200UI64     // Reserved for system use
#define POOL_FLAG_RESERVED3               0x0000000000000400UI64     // Reserved for system use
#define POOL_FLAG_REQUIRED_END            0x0000000080000000UI64

#define POOL_FLAG_OPTIONAL_START          0x0000000100000000UI64
#define POOL_FLAG_SPECIAL_POOL            0x0000000100000000UI64     // Make special pool allocation
#define POOL_FLAG_OPTIONAL_END            0x8000000000000000UI64

#define POOL_FLAG_REQUIRED_MASK           0x00000000FFFFFFFFUI64

#ifndef POOL_FLAG_LAST_KNOWN_REQUIRED
#define POOL_FLAG_LAST_KNOWN_REQUIRED     POOL_FLAG_RESERVED3        // Must be set to the last known required entry.
#endif

//
// Bits from the "required" flags that are currently not used.
//

#define POOL_FLAG_UNUSED_REQUIRED_BITS (POOL_FLAG_REQUIRED_MASK & ~(POOL_FLAG_LAST_KNOWN_REQUIRED | (POOL_FLAG_LAST_KNOWN_REQUIRED-1)))

typedef ULONG64 POOL_FLAGS;

// Using a special value (e.g. -2) made clang complain, so we invented this whole new class.
//
// Fortunately, everything is constexpr, so there should be no runtime bloat.
class CompatPOOL_TYPE {
    POOL_TYPE Value     = {};
    bool      Supported = {};

public:
    constexpr CompatPOOL_TYPE(): Supported(FALSE) { }
    constexpr CompatPOOL_TYPE(POOL_TYPE type): Value(type), Supported(TRUE) { }

    constexpr bool IsSupported() const { return Supported; }
    constexpr operator POOL_TYPE() const { return Value; }
};

consteval
CompatPOOL_TYPE
CompatPoolFlagsToPoolTypeImpl(
    POOL_FLAGS Flags
)
{
    constexpr CompatPOOL_TYPE Unsupported;

    CompatPOOL_TYPE Type = Unsupported;

    bool FlagUninitialized = false;
    bool FlagSession = false;
    bool FlagCacheAligned = false;

    if (Flags & POOL_FLAG_UNINITIALIZED)
    {
        FlagUninitialized = true;
        Flags ^= POOL_FLAG_UNINITIALIZED;
    }

    if (Flags & POOL_FLAG_SESSION)
    {
        FlagSession = true;
        Flags ^= POOL_FLAG_SESSION;
    }

    if (Flags & POOL_FLAG_CACHE_ALIGNED)
    {
        FlagCacheAligned = true;
        Flags ^= POOL_FLAG_CACHE_ALIGNED;
    }

    if (Flags & POOL_FLAG_NON_PAGED)
    {
        Flags ^= POOL_FLAG_NON_PAGED;
        constexpr CompatPOOL_TYPE Map[2][2] =
        {
            { NonPagedPoolNx, NonPagedPoolNxCacheAligned },
            { NonPagedPoolSessionNx, Unsupported }
        };
        Type = Map[FlagSession][FlagCacheAligned];
    }
    else if (Flags & POOL_FLAG_NON_PAGED_EXECUTE)
    {
        Flags ^= POOL_FLAG_NON_PAGED_EXECUTE;
        constexpr CompatPOOL_TYPE Map[2][2] =
        {
            { NonPagedPoolExecute, NonPagedPoolCacheAligned },
            { NonPagedPoolSession, NonPagedPoolCacheAlignedSession }
        };
        Type = Map[FlagSession][FlagCacheAligned];
    }
    else if (Flags & POOL_FLAG_PAGED)
    {
        Flags ^= POOL_FLAG_PAGED;
        constexpr CompatPOOL_TYPE Map[2][2] =
        {
            { PagedPool, PagedPoolCacheAligned },
            { PagedPoolSession, PagedPoolCacheAlignedSession }
        };
        Type = Map[FlagSession][FlagCacheAligned];
    }

    if ((Flags & POOL_FLAG_REQUIRED_MASK) != 0)
    {
        return Unsupported;
    }

    // No effect in ExAllocatePoolWithTag.
    // Instead, handled in compat macro below with a memset.
    UNREFERENCED_PARAMETER(FlagUninitialized);

    return Type;
}

// Newer SDKs define POOL_ZERO_ALLOCATION along with ExAllocatePoolZero.
// They also deprecate ExAllocatePoolWithTag, causing clang to complain.
// Meanwhile, older SDKs do not define ExAllocatePoolZero at all.

#ifdef POOL_ZERO_ALLOCATION
#define CompatAllocatePool ExAllocatePoolZero
constexpr bool CompatPoolNeedsManualZeroing = false;
#else
#define CompatAllocatePool ExAllocatePoolWithTag
constexpr bool CompatPoolNeedsManualZeroing = true;
#endif

#define ExAllocatePool2(Flags, NumberOfBytes, Tag)                                              \
    ([&] [[msvc::forceinline]] ()                                                               \
    {                                                                                           \
        constexpr CompatPOOL_TYPE Type = CompatPoolFlagsToPoolTypeImpl((Flags));                \
        static_assert(Type.IsSupported(), "Unsupported Pool Flags.");                           \
        PVOID pReturn = CompatAllocatePool((POOL_TYPE)Type, (NumberOfBytes), (Tag));            \
        if constexpr (!((Flags) & POOL_FLAG_UNINITIALIZED))                                     \
        {                                                                                       \
            if constexpr (CompatPoolNeedsManualZeroing)                                         \
            {                                                                                   \
                if (pReturn != NULL)                                                            \
                {                                                                               \
                    memset(pReturn, 0, NumberOfBytes);                                          \
                }                                                                               \
            }                                                                                   \
        }                                                                                       \
        return pReturn;                                                                         \
    })()

#endif

#if (NTDDI_VERSION < NTDDI_WIN10_RS2)
#define RtlGetNtSystemRoot()    (&SharedUserData->NtSystemRoot[0])
#endif

typedef struct _KADDRESS_RANGE_DESCRIPTOR
    KADDRESS_RANGE_DESCRIPTOR, *PKADDRESS_RANGE_DESCRIPTOR;

typedef enum _SUBSYSTEM_INFORMATION_TYPE SUBSYSTEM_INFORMATION_TYPE;
