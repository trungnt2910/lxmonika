#pragma once

#include <ntddk.h>
#include <intsafe.h>

#include "Locker.h"

#pragma warning(disable: 4201)

typedef union _PROCESS_HANDLER_INFORMATION {
    ULONG64 Value;
    struct {
        DWORD Handler;
        DWORD HasParentHandler : 1;
        DWORD HasInternalParentHandler : 1;
        DWORD ParentHandler : 30;
    } DUMMYSTRUCTNAME;
} PROCESS_HANDLER_INFORMATION, *PPROCESS_HANDLER_INFORMATION;

class ProcessMap
{
private:
    RTL_AVL_TABLE m_table;
    FAST_MUTEX m_mutex;

    typedef struct __NODE {
        PEPROCESS Key;
        PROCESS_HANDLER_INFORMATION Value;
    } _NODE, *_PNODE;

    PPROCESS_HANDLER_INFORMATION
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
        ProcessBelongsToHandler(
            _In_ PEPROCESS process,
            _In_ DWORD handler
        );
    NTSTATUS
        GetProcessHandler(
            _In_ PEPROCESS process,
            _Out_ DWORD* handler
        );
    NTSTATUS
        GetProcessHandler(
            _In_ PEPROCESS process,
            _Out_ PPROCESS_HANDLER_INFORMATION handlerInformation
        );
    NTSTATUS
        RegisterProcessHandler(
            _In_ PEPROCESS process,
            _In_ DWORD handler
        );
    NTSTATUS
        SwitchProcessHandler(
            _In_ PEPROCESS process,
            _In_ DWORD newHandler
        );
    NTSTATUS
        UnregisterProcess(
            _In_ PEPROCESS process
        );
    void Lock() { ExAcquireFastMutex(&m_mutex); }
    void Unlock() { ExReleaseFastMutex(&m_mutex); }
};
