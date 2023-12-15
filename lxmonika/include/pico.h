#pragma once

#include <ntddk.h>

// pico.h
//
// Support definitions for Windows NT Pico processes.
//
// Based on https://github.com/thinkcz/pico-toolbox/blob/master/ToolBoxDriver/picostruct.h
// Released by Martin Hron a.k.a. thinkcz (martin@hron.eu) under the MIT license.
//
// According to https://speakerdeck.com/ntddk/an-introduction-to-drawbridge-ja?slide=36,
// this is in turn based on ntosp.h, a header that was accidentally released in
// older versions of the Windows 10 SDK.

#ifdef __cplusplus
extern "C"
{
#endif

// create process flags
#define PS_PICO_CREATE_PROCESS_CLONE_PARENT             0x1
#define PS_PICO_CREATE_PROCESS_INHERIT_HANDLES          0x2
#define PS_PICO_CREATE_PROCESS_CLONE_REDUCED_COMMIT     0x4
#define PS_PICO_CREATE_PROCESS_BREAKAWAY                0x8
#define PS_PICO_CREATE_PROCESS_PACKAGED_PROCESS         0x10

#define PS_PICO_CREATE_PROCESS_FLAGS_MASK (         \
    PS_PICO_CREATE_PROCESS_CLONE_PARENT         |   \
    PS_PICO_CREATE_PROCESS_INHERIT_HANDLES      |   \
    PS_PICO_CREATE_PROCESS_CLONE_REDUCED_COMMIT |   \
    PS_PICO_CREATE_PROCESS_BREAKAWAY            |   \
    PS_PICO_CREATE_PROCESS_PACKAGED_PROCESS)

typedef struct _PS_PICO_PROCESS_ATTRIBUTES {
    HANDLE ParentProcess;
    HANDLE Token;
    PVOID Context;
    ULONG Flags;
} PS_PICO_PROCESS_ATTRIBUTES, *PPS_PICO_PROCESS_ATTRIBUTES;

typedef struct _PS_PICO_PROCESS_CREATE_CONTEXT {
    PFILE_OBJECT ImageFile;
    PUNICODE_STRING* ImageName;
    PUNICODE_STRING* CommandLine;
} PS_PICO_PROCESS_CREATE_CONTEXT, *PPS_PICO_PROCESS_CREATE_CONTEXT;

typedef struct _PS_PICO_THREAD_ATTRIBUTES {
    HANDLE Process;
    ULONG_PTR UserStack;
    ULONG_PTR StartRoutine;
    ULONG_PTR StartParameter1;
    ULONG_PTR StartParameter2;

#if defined(_AMD64_)

    ULONG UserFsBase;
    ULONG64 UserGsBase;
    ULONG_PTR Rax;
    ULONG_PTR Rcx;
    ULONG_PTR Rdx;
    ULONG_PTR Rbx;
    ULONG_PTR Rsp;
    ULONG_PTR Rbp;
    ULONG_PTR Rsi;
    ULONG_PTR Rdi;
    ULONG_PTR R8;
    ULONG_PTR R9;
    ULONG_PTR R10;
    ULONG_PTR R11;
    ULONG_PTR R12;
    ULONG_PTR R13;
    ULONG_PTR R14;
    ULONG_PTR R15;

#elif defined(_X86_)

    ULONG UserFsBase;
    ULONG UserGsBase;

    USHORT UserFsSeg;
    USHORT UserGsSeg;

    ULONG_PTR Eax;
    ULONG_PTR Ebx;
    ULONG_PTR Ecx;
    ULONG_PTR Edx;
    ULONG_PTR Edi;
    ULONG_PTR Esi;
    ULONG_PTR Ebp;

#elif defined(_ARM_)

    ULONG UserRoBase;
    ULONG UserRwBase;

    ULONG Lr;
    ULONG R2;
    ULONG R3;
    ULONG R4;
    ULONG R5;
    ULONG R6;
    ULONG R7;
    ULONG R8;
    ULONG R9;
    ULONG R10;
    ULONG R11;
    ULONG R12;

#endif

    PVOID Context;

} PS_PICO_THREAD_ATTRIBUTES, *PPS_PICO_THREAD_ATTRIBUTES;


// prototypes of PICO functions

typedef NTSTATUS PS_PICO_CREATE_PROCESS(
    _In_ PPS_PICO_PROCESS_ATTRIBUTES ProcessAttributes,
    _In_opt_ PPS_PICO_PROCESS_CREATE_CONTEXT ProcessCreateContext,
    _Outptr_ PHANDLE ProcessHandle
);

typedef PS_PICO_CREATE_PROCESS* PPS_PICO_CREATE_PROCESS;

typedef NTSTATUS PS_PICO_CREATE_THREAD(
    _In_ PPS_PICO_THREAD_ATTRIBUTES ThreadAttributes,
    _In_opt_ PPS_PICO_PROCESS_CREATE_CONTEXT ProcessCreateContext,
    _Outptr_ PHANDLE ThreadHandle
);
typedef PS_PICO_CREATE_THREAD* PPS_PICO_CREATE_THREAD;

typedef PVOID PS_PICO_GET_PROCESS_CONTEXT(
    _In_ PEPROCESS Process
);
typedef PS_PICO_GET_PROCESS_CONTEXT* PPS_PICO_GET_PROCESS_CONTEXT;


typedef PVOID PS_PICO_GET_THREAD_CONTEXT(
    _In_ PETHREAD Thread
);
typedef PS_PICO_GET_THREAD_CONTEXT* PPS_PICO_GET_THREAD_CONTEXT;

typedef enum _PS_PICO_THREAD_DESCRIPTOR_TYPE {

#if defined(_X86_) || defined(_AMD64_)

    PicoThreadDescriptorTypeFs,
    PicoThreadDescriptorTypeGs,

#elif defined(_ARM_)

    PicoThreadDescriptorTypeUserRo,
    PicoThreadDescriptorTypeUserRw,

#endif

    PicoThreadDescriptorTypeMax
} PS_PICO_THREAD_DESCRIPTOR_TYPE, *PPS_PICO_THREAD_DESCRIPTOR_TYPE;

typedef VOID PS_PICO_SET_THREAD_DESCRIPTOR_BASE(
    _In_ PS_PICO_THREAD_DESCRIPTOR_TYPE Type,
    _In_ ULONG_PTR Base
);
typedef PS_PICO_SET_THREAD_DESCRIPTOR_BASE* PPS_PICO_SET_THREAD_DESCRIPTOR_BASE;

typedef NTSTATUS PS_PICO_TERMINATE_PROCESS(
    __inout PEPROCESS Process,
    __in NTSTATUS ExitStatus
);
typedef PS_PICO_TERMINATE_PROCESS* PPS_PICO_TERMINATE_PROCESS;

typedef NTSTATUS PS_SET_CONTEXT_THREAD_INTERNAL(
    __in PETHREAD Thread,
    __in PCONTEXT ThreadContext,
    __in KPROCESSOR_MODE ProbeMode,
    __in KPROCESSOR_MODE CtxMode,
    __in BOOLEAN PerformUnwind
);
typedef PS_SET_CONTEXT_THREAD_INTERNAL* PPS_SET_CONTEXT_THREAD_INTERNAL;

typedef NTSTATUS PS_GET_CONTEXT_THREAD_INTERNAL(
    __in PETHREAD Thread,
    __inout PCONTEXT ThreadContext,
    __in KPROCESSOR_MODE ProbeMode,
    __in KPROCESSOR_MODE CtxMode,
    __in BOOLEAN PerformUnwind
);
typedef PS_GET_CONTEXT_THREAD_INTERNAL* PPS_GET_CONTEXT_THREAD_INTERNAL;


typedef NTSTATUS PS_TERMINATE_THREAD(
    __inout PETHREAD Thread,
    __in NTSTATUS ExitStatus,
    __in BOOLEAN DirectTerminate
);
typedef PS_TERMINATE_THREAD* PPS_TERMINATE_THREAD;


typedef NTSTATUS PS_SUSPEND_THREAD(
    _In_ PETHREAD Thread,
    _Out_opt_ PULONG PreviousSuspendCount
);
typedef PS_SUSPEND_THREAD* PPS_SUSPEND_THREAD;

PS_SUSPEND_THREAD PsSuspendThread;

typedef NTSTATUS PS_RESUME_THREAD(
    _In_ PETHREAD Thread,
    _Out_opt_ PULONG PreviousSuspendCount
);
typedef PS_RESUME_THREAD* PPS_RESUME_THREAD;

PS_RESUME_THREAD PsResumeThread;


typedef struct _PS_PICO_ROUTINES {
    SIZE_T Size;
    PPS_PICO_CREATE_PROCESS CreateProcess;
    PPS_PICO_CREATE_THREAD CreateThread;
    PPS_PICO_GET_PROCESS_CONTEXT GetProcessContext;
    PPS_PICO_GET_THREAD_CONTEXT GetThreadContext;
    PPS_GET_CONTEXT_THREAD_INTERNAL GetContextThreadInternal;
    PPS_SET_CONTEXT_THREAD_INTERNAL SetContextThreadInternal;
    PPS_TERMINATE_THREAD TerminateThread;
    PPS_RESUME_THREAD ResumeThread;
    PPS_PICO_SET_THREAD_DESCRIPTOR_BASE SetThreadDescriptorBase;
    PPS_SUSPEND_THREAD SuspendThread;
    PPS_PICO_TERMINATE_PROCESS TerminateProcess;
} PS_PICO_ROUTINES, *PPS_PICO_ROUTINES;

typedef struct _PS_PICO_SYSTEM_CALL_INFORMATION {
    PKTRAP_FRAME TrapFrame;

#if defined(_ARM_)

    ULONG R4;
    ULONG R5;
    ULONG R7;

#endif

} PS_PICO_SYSTEM_CALL_INFORMATION, *PPS_PICO_SYSTEM_CALL_INFORMATION;

#if defined(_ARM_)

//
// Structure offsets known to assembler code that does not use genxx, verify
// that the offsets are the same for PsPicoSystemCallDispatch.
//

C_ASSERT(FIELD_OFFSET(PS_PICO_SYSTEM_CALL_INFORMATION, TrapFrame) == 0x00);
C_ASSERT(FIELD_OFFSET(PS_PICO_SYSTEM_CALL_INFORMATION, R4) == 0x04);
C_ASSERT(FIELD_OFFSET(PS_PICO_SYSTEM_CALL_INFORMATION, R5) == 0x08);
C_ASSERT(FIELD_OFFSET(PS_PICO_SYSTEM_CALL_INFORMATION, R7) == 0x0C);

#endif



typedef
VOID
PS_PICO_PROVIDER_SYSTEM_CALL_DISPATCH(
    _In_ PPS_PICO_SYSTEM_CALL_INFORMATION SystemCall
);

typedef PS_PICO_PROVIDER_SYSTEM_CALL_DISPATCH* PPS_PICO_PROVIDER_SYSTEM_CALL_DISPATCH;

typedef
VOID
PS_PICO_PROVIDER_THREAD_EXIT(
    _In_ PETHREAD Thread
);

typedef PS_PICO_PROVIDER_THREAD_EXIT* PPS_PICO_PROVIDER_THREAD_EXIT;

typedef
VOID
PS_PICO_PROVIDER_PROCESS_EXIT(
    _In_ PEPROCESS Process
);

typedef PS_PICO_PROVIDER_PROCESS_EXIT* PPS_PICO_PROVIDER_PROCESS_EXIT;

typedef
BOOLEAN
PS_PICO_PROVIDER_DISPATCH_EXCEPTION(
    _Inout_ PEXCEPTION_RECORD ExceptionRecord,
    _Inout_ PKEXCEPTION_FRAME ExceptionFrame,
    _Inout_ PKTRAP_FRAME TrapFrame,
    _In_ ULONG Chance,
    _In_ KPROCESSOR_MODE PreviousMode
);

typedef PS_PICO_PROVIDER_DISPATCH_EXCEPTION* PPS_PICO_PROVIDER_DISPATCH_EXCEPTION;

typedef
NTSTATUS
PS_PICO_PROVIDER_TERMINATE_PROCESS(
    _In_ PEPROCESS Process,
    _In_ NTSTATUS TerminateStatus
);

typedef PS_PICO_PROVIDER_TERMINATE_PROCESS* PPS_PICO_PROVIDER_TERMINATE_PROCESS;

typedef
_Ret_range_(<= , FrameCount)
ULONG
PS_PICO_PROVIDER_WALK_USER_STACK(
    _In_ PKTRAP_FRAME TrapFrame,
    _Out_writes_to_(FrameCount, return) PVOID* Callers,
    _In_ ULONG FrameCount
);

typedef PS_PICO_PROVIDER_WALK_USER_STACK* PPS_PICO_PROVIDER_WALK_USER_STACK;

typedef
NTSTATUS
PS_PICO_GET_ALLOCATED_PROCESS_IMAGE_NAME(
    _In_ PEPROCESS Process,
    _Outptr_ PUNICODE_STRING* ImageName
);

typedef PS_PICO_GET_ALLOCATED_PROCESS_IMAGE_NAME* PPS_PICO_GET_ALLOCATED_PROCESS_IMAGE_NAME;

typedef struct _PS_PICO_PROVIDER_ROUTINES {
    SIZE_T Size;
    PPS_PICO_PROVIDER_SYSTEM_CALL_DISPATCH DispatchSystemCall;
    PPS_PICO_PROVIDER_THREAD_EXIT ExitThread;
    PPS_PICO_PROVIDER_PROCESS_EXIT ExitProcess;
    PPS_PICO_PROVIDER_DISPATCH_EXCEPTION DispatchException;
    PPS_PICO_PROVIDER_TERMINATE_PROCESS TerminateProcess;
    PPS_PICO_PROVIDER_WALK_USER_STACK WalkUserStack;
    CONST KADDRESS_RANGE_DESCRIPTOR* ProtectedRanges;
    PPS_PICO_GET_ALLOCATED_PROCESS_IMAGE_NAME GetAllocatedProcessImageName;
    ACCESS_MASK OpenProcess;
    ACCESS_MASK OpenThread;
    SUBSYSTEM_INFORMATION_TYPE SubsystemInformationType;
} PS_PICO_PROVIDER_ROUTINES, *PPS_PICO_PROVIDER_ROUTINES;

//
// Process and Thread Security and Access Rights
//
// Based on winnt.h
// See also
// https://learn.microsoft.com/en-us/windows/win32/procthread/process-security-and-access-rights
// and https://learn.microsoft.com/en-us/windows/win32/procthread/thread-security-and-access-rights
//

#define PROCESS_TERMINATE                  (0x0001)
#define PROCESS_CREATE_THREAD              (0x0002)
#define PROCESS_SET_SESSIONID              (0x0004)
#define PROCESS_VM_OPERATION               (0x0008)
#define PROCESS_VM_READ                    (0x0010)
#define PROCESS_VM_WRITE                   (0x0020)
#define PROCESS_DUP_HANDLE                 (0x0040)
#define PROCESS_CREATE_PROCESS             (0x0080)
#define PROCESS_SET_QUOTA                  (0x0100)
#define PROCESS_SET_INFORMATION            (0x0200)
#define PROCESS_QUERY_INFORMATION          (0x0400)
#define PROCESS_SUSPEND_RESUME             (0x0800)
#define PROCESS_QUERY_LIMITED_INFORMATION  (0x1000)
#define PROCESS_SET_LIMITED_INFORMATION    (0x2000)
//

#if (NTDDI_VERSION >= NTDDI_VISTA)
#define PROCESS_ALL_ACCESS        (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | \
                                   0xFFFF)
#else
#define PROCESS_ALL_ACCESS        (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | \
                                   0xFFF)
#endif

//
#define THREAD_TERMINATE                 (0x0001)
#define THREAD_SUSPEND_RESUME            (0x0002)
#define THREAD_GET_CONTEXT               (0x0008)
#define THREAD_SET_CONTEXT               (0x0010)
#define THREAD_QUERY_INFORMATION         (0x0040)
#define THREAD_SET_INFORMATION           (0x0020)
#define THREAD_SET_THREAD_TOKEN          (0x0080)
#define THREAD_IMPERSONATE               (0x0100)
#define THREAD_DIRECT_IMPERSONATION      (0x0200)
#define THREAD_SET_LIMITED_INFORMATION   (0x0400)  // winnt
#define THREAD_QUERY_LIMITED_INFORMATION (0x0800)  // winnt
#define THREAD_RESUME                    (0x1000)  // winnt

#if (NTDDI_VERSION >= NTDDI_VISTA)
#define THREAD_ALL_ACCESS         (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | \
                                   0xFFFF)
#else
#define THREAD_ALL_ACCESS         (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | \
                                   0x3FF)
#endif

#ifdef __cplusplus
}
#endif
