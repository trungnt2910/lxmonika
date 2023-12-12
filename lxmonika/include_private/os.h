#pragma once

#include <ntddk.h>

// os.h
//
// Support definitions for Windows NT OS internal functions.

#pragma warning(disable: 4201)

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum _SECTION_INFORMATION_CLASS {
    SectionBasicInformation,
    SectionImageInformation
} SECTION_INFORMATION_CLASS;

typedef struct _SECTION_IMAGE_INFORMATION
{
    PVOID   TransferAddress;
    ULONG   ZeroBits;
    SIZE_T  MaximumStackSize;
    SIZE_T  CommittedStackSize;
    ULONG   SubSystemType;
    union
    {
        struct DUMMYUNIONNAME
        {
            USHORT SubSystemMinorVersion;
            USHORT SubSystemMajorVersion;
        };
        ULONG SubSystemVersion;
    };
    ULONG   GpValue;
    USHORT  ImageCharacteristics;
    USHORT  DllCharacteristics;
    USHORT  Machine;
    UCHAR   ImageContainsCode;
    UCHAR   Spare1;
    ULONG   LoaderFlags;
    ULONG   ImageFileSize;
    ULONG   Reserved[1];
} SECTION_IMAGE_INFORMATION, *PSECTION_IMAGE_INFORMATION;

typedef enum _SYSTEM_INFORMATION_CLASS {
    SystemModuleInformation = 11,
    SystemBigPoolInformation = 66
} SYSTEM_INFORMATION_CLASS;

typedef struct _RTL_PROCESS_MODULE_INFORMATION {
    HANDLE  Section;                 // Not filled in
    PVOID   MappedBase;
    PVOID   ImageBase;
    ULONG   ImageSize;
    ULONG   Flags;
    USHORT  LoadOrderIndex;
    USHORT  InitOrderIndex;
    USHORT  LoadCount;
    USHORT  OffsetToFileName;
    UCHAR   FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES {
    ULONG                           NumberOfModules;
    RTL_PROCESS_MODULE_INFORMATION  Modules[1];
} RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES;

typedef struct _SYSTEM_BIGPOOL_ENTRY {
    union
    {
        PVOID       VirtualAddress;
        ULONG_PTR   NonPaged : 1;
    };

    ULONG_PTR   SizeInBytes;

    union
    {
        UCHAR   Tag[4];
        ULONG   TagUlong;
    };
} SYSTEM_BIGPOOL_ENTRY, *PSYSTEM_BIGPOOL_ENTRY;

typedef struct _SYSTEM_BIGPOOL_INFORMATION {
    ULONG                   Count;
    SYSTEM_BIGPOOL_ENTRY    AllocatedInfo[ANYSIZE_ARRAY];
} SYSTEM_BIGPOOL_INFORMATION, *PSYSTEM_BIGPOOL_INFORMATION;

__declspec(dllimport)
NTSTATUS
    ZwQuerySection(
        _In_        HANDLE SectionHandle,
        _In_        SECTION_INFORMATION_CLASS InformationClass,
        _Out_       PVOID InformationBuffer,
        _In_        ULONG InformationBufferSize,
        _Out_opt_   PULONG ResultLength
    );

__declspec(dllimport)
NTSTATUS
    ZwQuerySystemInformation(
        _In_        SYSTEM_INFORMATION_CLASS SystemInformationClass,
        _In_        PVOID SystemInformation,
        _In_        ULONG SystemInformationLength,
        _Out_       PULONG ReturnLength
    );

__declspec(dllimport)
PEPROCESS
    PsGetThreadProcess(
        _In_        PETHREAD Thread
    );

#ifdef __cplusplus
}
#endif
