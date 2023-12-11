#pragma once

#include <ntddk.h>
#include <intsafe.h>

#pragma warning(disable: 4201)

typedef union _PROCESS_PROVIDER_INFORMATION {
    ULONG64 Value;
    struct {
        DWORD HasProvider : 1;
        DWORD ProviderId : 31;
        DWORD HasParentProvider : 1;
        DWORD HasInternalParentProvider : 1;
        DWORD ParentProviderId : 30;
    } DUMMYSTRUCTNAME;
} PROCESS_PROVIDER_INFORMATION, *PPROCESS_PROVIDER_INFORMATION;

#define PROCESS_PROVIDER_NULL 0

class ProcessMap
{
private:
    RTL_AVL_TABLE m_table;
    FAST_MUTEX m_mutex;

    typedef struct __NODE {
        PEPROCESS Key;
        PROCESS_PROVIDER_INFORMATION Value;
    } _NODE, *_PNODE;

    PPROCESS_PROVIDER_INFORMATION
        operator[](
            _In_ PEPROCESS key
            );

    static RTL_GENERIC_COMPARE_RESULTS
        _CompareRoutine(
            _In_ PRTL_AVL_TABLE Table,
            _In_ PVOID FirstStruct,
            _In_ PVOID SecondStruct
        );
    static PVOID
        _AllocateRoutine(
            _In_ PRTL_AVL_TABLE Table,
            _In_ CLONG ByteSize
        );
    static VOID
        _FreeRoutine(
            _In_ PRTL_AVL_TABLE Table,
            _In_ PVOID Buffer
        );
public:
    ProcessMap() = default;

    VOID
        Initialize();

    VOID
        Clear();

    BOOLEAN
        ProcessBelongsToProvider(
            _In_ PEPROCESS process,
            _In_ DWORD provider
        );
    NTSTATUS
        GetProcessProvider(
            _In_ PEPROCESS process,
            _Out_ DWORD* provider
        );
    NTSTATUS
        GetProcessProvider(
            _In_ PEPROCESS process,
            _Out_ PPROCESS_PROVIDER_INFORMATION providerInformation
        );
    NTSTATUS
        RegisterProcessProvider(
            _In_ PEPROCESS process,
            _In_ DWORD provider
        );
    NTSTATUS
        SwitchProcessProvider(
            _In_ PEPROCESS process,
            _In_ DWORD newProvider
        );
    NTSTATUS
        UnregisterProcess(
            _In_ PEPROCESS process
        );
    void Lock() { ExAcquireFastMutex(&m_mutex); }
    void Unlock() { ExReleaseFastMutex(&m_mutex); }
};
