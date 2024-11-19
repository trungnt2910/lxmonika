#pragma once

#include <ntifs.h>

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


#if defined(_M_IX86)

// begin_nthal
//
// Trap frame
//
//  NOTE - We deal only with 32bit registers, so the assembler equivalents
//         are always the extended forms.
//
//  NOTE - Unless you want to run like slow molasses everywhere in the
//         the system, this structure must be of DWORD length, DWORD
//         aligned, and its elements must all be DWORD aligned.
//
//  NOTE WELL   -
//
//      The i386 does not build stack frames in a consistent format, the
//      frames vary depending on whether or not a privilege transition
//      was involved.
//
//      In order to make NtContinue work for both user mode and kernel
//      mode callers, we must force a canonical stack.
//
//      If we're called from kernel mode, this structure is 8 bytes longer
//      than the actual frame!
//
//  WARNING:
//
//      KTRAP_FRAME_LENGTH needs to be 16byte integral (at present.)
//

typedef struct _KTRAP_FRAME {


//
//  Following 4 values are only used and defined for DBG systems,
//  but are always allocated to make switching from DBG to non-DBG
//  and back quicker.  They are not DEVL because they have a non-0
//  performance impact.
//

    ULONG   DbgEbp;         // Copy of User EBP set up so KB will work.
    ULONG   DbgEip;         // EIP of caller to system call, again, for KB.
    ULONG   DbgArgMark;     // Marker to show no args here.

//
//  Temporary values used when frames are edited.
//
//
//  NOTE:   Any code that want's ESP must materialize it, since it
//          is not stored in the frame for kernel mode callers.
//
//          And code that sets ESP in a KERNEL mode frame, must put
//          the new value in TempEsp, make sure that TempSegCs holds
//          the real SegCs value, and put a special marker value into SegCs.
//

    USHORT  TempSegCs;
    UCHAR   Logging;
    UCHAR   FrameType;
    ULONG   TempEsp;

//
//  Debug registers.
//

    ULONG   Dr0;
    ULONG   Dr1;
    ULONG   Dr2;
    ULONG   Dr3;
    ULONG   Dr6;
    ULONG   Dr7;

//
//  Segment registers
//

    ULONG   SegGs;
    ULONG   SegEs;
    ULONG   SegDs;

//
//  Volatile registers
//

    ULONG   Edx;
    ULONG   Ecx;
    ULONG   Eax;

//
//  Nesting state, not part of context record
//

    UCHAR   PreviousPreviousMode;
    UCHAR   EntropyQueueDpc;
    UCHAR   Reserved[2];

    ULONG   MxCsr;

    PEXCEPTION_REGISTRATION_RECORD ExceptionList;
                                            // Trash if caller was user mode.
                                            // Saved exception list if caller
                                            // was kernel mode or we're in
                                            // an interrupt.

//
//  FS is TIB/PCR pointer, is here to make save sequence easy
//

    ULONG   SegFs;

//
//  Non-volatile registers
//

    ULONG   Edi;
    ULONG   Esi;
    ULONG   Ebx;
    ULONG   Ebp;

//
//  Control registers
//

    ULONG   ErrCode;
    ULONG   Eip;
    ULONG   SegCs;
    ULONG   EFlags;

    ULONG   HardwareEsp;    // WARNING - segSS:esp are only here for stacks
    ULONG   HardwareSegSs;  // that involve a ring transition.

    ULONG   V86Es;          // these will be present for all transitions from
    ULONG   V86Ds;          // V86 mode
    ULONG   V86Fs;
    ULONG   V86Gs;
} KTRAP_FRAME;


typedef KTRAP_FRAME *PKTRAP_FRAME;
typedef KTRAP_FRAME *PKEXCEPTION_FRAME;

// end_nthal

#endif // defined(_M_IX86)


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

typedef struct _PS_PICO_CREATE_INFO {
    PFILE_OBJECT FileObject;
    PUNICODE_STRING ImageFileName;
    PUNICODE_STRING CommandLine;
} PS_PICO_CREATE_INFO, *PPS_PICO_CREATE_INFO;

typedef struct _PS_PICO_THREAD_ATTRIBUTES {
    HANDLE Process;
    ULONG_PTR UserStack;
    ULONG_PTR StartRoutine;
    ULONG_PTR StartParameter1;
    ULONG_PTR StartParameter2;

#if defined(_M_X64)

    ULONG64 UserFsBase;
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

#elif defined(_M_IX86)

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

#elif defined(_M_ARM32)

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

#elif defined(_M_ARM64)

    ULONG64 UserRoBase;
    ULONG64 UserRwBase;

    ULONG_PTR X2;
    ULONG_PTR X3;
    ULONG_PTR X4;
    ULONG_PTR X5;
    ULONG_PTR X6;
    ULONG_PTR X7;
    ULONG_PTR X8;
    ULONG_PTR X9;
    ULONG_PTR X10;
    ULONG_PTR X11;
    ULONG_PTR X12;
    ULONG_PTR X13;
    ULONG_PTR X14;
    ULONG_PTR X15;
    ULONG_PTR X16;
    ULONG_PTR X17;
    ULONG_PTR X18;
    ULONG_PTR X19;
    ULONG_PTR X20;
    ULONG_PTR X21;
    ULONG_PTR X22;
    ULONG_PTR X23;
    ULONG_PTR X24;
    ULONG_PTR X25;
    ULONG_PTR X26;
    ULONG_PTR X27;
    ULONG_PTR X28;
    ULONG_PTR Fp;
    ULONG_PTR Lr;

#endif

    PVOID Context;

} PS_PICO_THREAD_ATTRIBUTES, *PPS_PICO_THREAD_ATTRIBUTES;


// prototypes of PICO functions

typedef NTSTATUS PS_PICO_CREATE_PROCESS_TH1(
    _In_ PPS_PICO_PROCESS_ATTRIBUTES ProcessAttributes,
    _Outptr_ PHANDLE ProcessHandle
);
typedef NTSTATUS PS_PICO_CREATE_PROCESS_RS4(
    _In_ PPS_PICO_PROCESS_ATTRIBUTES ProcessAttributes,
    _In_opt_ PPS_PICO_CREATE_INFO CreateInfo,
    _Outptr_ PHANDLE ProcessHandle
);

#if (NTDDI_VERSION >= NTDDI_RS4)
typedef PS_PICO_CREATE_PROCESS_RS4 PS_PICO_CREATE_PROCESS;
#else
typedef PS_PICO_CREATE_PROCESS_TH1 PS_PICO_CREATE_PROCESS;
#endif

typedef PS_PICO_CREATE_PROCESS* PPS_PICO_CREATE_PROCESS;


typedef NTSTATUS PS_PICO_CREATE_THREAD_TH1(
    _In_ PPS_PICO_THREAD_ATTRIBUTES ThreadAttributes,
    _Outptr_ PHANDLE ThreadHandle
);
typedef NTSTATUS PS_PICO_CREATE_THREAD_RS2(
    _In_ PPS_PICO_THREAD_ATTRIBUTES ThreadAttributes,
    _In_opt_ PPS_PICO_CREATE_INFO CreateInfo,
    _Outptr_ PHANDLE ThreadHandle
);

#if (NTDDI_VERSION >= NTDDI_RS2)
typedef PS_PICO_CREATE_THREAD_RS2 PS_PICO_CREATE_THREAD;
#else
typedef PS_PICO_CREATE_THREAD_TH1 PS_PICO_CREATE_THREAD;
#endif

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

#if defined(_M_IX86) || defined(_M_X64)

    PicoThreadDescriptorTypeFs,
    PicoThreadDescriptorTypeGs,

#elif defined(_M_ARM) || defined(_M_ARM64)

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

#if defined(_M_ARM)

    ULONG R4;
    ULONG R5;
    ULONG R7;

#endif

} PS_PICO_SYSTEM_CALL_INFORMATION, *PPS_PICO_SYSTEM_CALL_INFORMATION;

#if defined(_M_ARM)

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

#if (NTDDI_VERSION >= NTDDI_THRESHOLD)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTKERNELAPI
NTSTATUS
PsRegisterPicoProvider(
    _In_ PPS_PICO_PROVIDER_ROUTINES ProviderRoutines,
    _Inout_ PPS_PICO_ROUTINES PicoRoutines
);
#endif

#ifdef __cplusplus
}
#endif
