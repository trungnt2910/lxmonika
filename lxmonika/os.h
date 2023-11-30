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
    PVOID	TransferAddress;
    ULONG	ZeroBits;
    SIZE_T	MaximumStackSize;
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
    SystemModuleInformation = 11
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

NTSTATUS
ZwQuerySection(
    IN HANDLE SectionHandle,
    IN SECTION_INFORMATION_CLASS InformationClass,
    OUT PVOID InformationBuffer,
    IN ULONG InformationBufferSize,
    OUT PULONG ResultLength OPTIONAL);

NTSTATUS
ZwQuerySystemInformation(
    IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
    IN PVOID SystemInformation,
    IN ULONG SystemInformationLength,
    OUT PULONG ReturnLength);

#ifdef __cplusplus
}
#endif
