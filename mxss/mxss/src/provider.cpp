#include "provider.h"

#include <monika.h>

#include "console.h"
#include "process.h"
#include "syscall.h"
#include "thread.h"

#include "AutoResource.h"

PS_PICO_ROUTINES MxRoutines
{
    .Size = sizeof(PS_PICO_ROUTINES)
};

MA_PICO_ROUTINES MxAdditionalRoutines
{
    .Size = sizeof(MA_PICO_ROUTINES)
};

#define MX_RETURN_IF_FAIL(s)        \
    do                              \
    {                               \
        NTSTATUS status__ = (s);    \
        if (!NT_SUCCESS(status__))  \
            return status__;        \
    }                               \
    while (FALSE)

extern "C"
VOID
MxSystemCallDispatch(
    _In_ PPS_PICO_SYSTEM_CALL_INFORMATION SystemCall
)
{
#ifdef _M_AMD64
    INT iSysNum = (INT)SystemCall->TrapFrame->Rax;
    UINT_PTR uArg1 = SystemCall->TrapFrame->Rdi;
    UINT_PTR uArg2 = SystemCall->TrapFrame->Rsi;
    UINT_PTR uArg3 = SystemCall->TrapFrame->Rdx;
    UINT_PTR uArg4 = SystemCall->TrapFrame->R10;
    UINT_PTR uArg5 = SystemCall->TrapFrame->R8;
    UINT_PTR uArg6 = SystemCall->TrapFrame->R9;
#elif defined(_M_ARM64)
    INT iSysNum = (INT)SystemCall->TrapFrame->X8;
    UINT_PTR uArg1 = SystemCall->TrapFrame->X0;
    UINT_PTR uArg2 = SystemCall->TrapFrame->X1;
    UINT_PTR uArg3 = SystemCall->TrapFrame->X2;
    UINT_PTR uArg4 = SystemCall->TrapFrame->X3;
    UINT_PTR uArg5 = SystemCall->TrapFrame->X4;
    UINT_PTR uArg6 = SystemCall->TrapFrame->X5;
#else
#error Detect the syscall arguments for this architecture!
#endif

    INT_PTR iRet = -1;

    UNREFERENCED_PARAMETER(uArg1);
    UNREFERENCED_PARAMETER(uArg2);
    UNREFERENCED_PARAMETER(uArg3);
    UNREFERENCED_PARAMETER(uArg4);
    UNREFERENCED_PARAMETER(uArg5);
    UNREFERENCED_PARAMETER(uArg6);

    PMX_THREAD pMxThread = (PMX_THREAD)MxRoutines.GetThreadContext(PsGetCurrentThread());
    if (pMxThread != NULL)
    {
        pMxThread->CurrentSystemCall = SystemCall;
    }

    // Syscall numbers are defined here:
    // https://github.com/itsmevjnk/sysx/blob/main/exec/syscall.h
    switch (iSysNum)
    {
        case SYSCALL_EXIT:  // 0
        {
            iRet = SyscallExit((INT)uArg1);
        }
        break;
        case SYSCALL_READ:  // 1
        {
            DbgBreakPoint();
        }
        break;
        case SYSCALL_WRITE: // 2
        {
            iRet = SyscallWrite((INT)uArg1, (PVOID)uArg2, (SIZE_T)uArg3);
        }
        break;
        case SYSCALL_FORK:  // 3
        {
            iRet = SyscallFork();
        }
        break;
        default:
            KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
                "Unimplemented syscall: %x\n", iSysNum));
    }

#ifdef _M_AMD64
    SystemCall->TrapFrame->Rax = (ULONG64)iRet;
#elif defined(_M_ARM64)
    SystemCall->TrapFrame->X0 = (ULONG64)iRet;
#else
#error Set the syscall return value for this architecture!
#endif

    if (pMxThread != NULL)
    {
        pMxThread->CurrentSystemCall = NULL;
    }
}

extern "C"
VOID
MxThreadExit(
    _In_ PETHREAD Thread
)
{
    UNREFERENCED_PARAMETER(Thread);
}

extern "C"
VOID
MxProcessExit(
    _In_ PEPROCESS Process
)
{
    UNREFERENCED_PARAMETER(Process);
}

extern "C"
BOOLEAN
MxDispatchException(
    _Inout_ PEXCEPTION_RECORD ExceptionRecord,
    _Inout_ PKEXCEPTION_FRAME ExceptionFrame,
    _Inout_ PKTRAP_FRAME TrapFrame,
    _In_ ULONG Chance,
    _In_ KPROCESSOR_MODE PreviousMode
)
{
    UNREFERENCED_PARAMETER(ExceptionRecord);
    UNREFERENCED_PARAMETER(ExceptionFrame);
    UNREFERENCED_PARAMETER(TrapFrame);
    UNREFERENCED_PARAMETER(Chance);
    UNREFERENCED_PARAMETER(PreviousMode);

    return FALSE;
}

extern "C"
NTSTATUS
MxTerminateProcess(
    _In_ PEPROCESS Process,
    _In_ NTSTATUS TerminateStatus
)
{
    UNREFERENCED_PARAMETER(Process);
    UNREFERENCED_PARAMETER(TerminateStatus);

    return STATUS_NOT_IMPLEMENTED;
}

extern "C"
_Ret_range_(<= , FrameCount)
ULONG
MxWalkUserStack(
    _In_ PKTRAP_FRAME TrapFrame,
    _Out_writes_to_(FrameCount, return) PVOID* Callers,
    _In_ ULONG FrameCount
)
{
    UNREFERENCED_PARAMETER(TrapFrame);
    UNREFERENCED_PARAMETER(Callers);
    UNREFERENCED_PARAMETER(FrameCount);

    return 0;
}

static UNICODE_STRING MxProviderName = RTL_CONSTANT_STRING(L"Monix-0.0.1-prealpha");

extern "C"
NTSTATUS
MxGetAllocatedProviderName(
    _Outptr_ PUNICODE_STRING* pOutProviderName
)
{
    *pOutProviderName = &MxProviderName;
    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
MxGetConsole(
    _In_ PEPROCESS Process,
    _Out_opt_ PHANDLE Console,
    _Out_opt_ PHANDLE Input,
    _Out_opt_ PHANDLE Output
)
{
    PMX_PROCESS pMxProcess = (PMX_PROCESS)MxRoutines.GetProcessContext(Process);

    HANDLE hdlConsole = NULL;
    MX_RETURN_IF_FAIL(CoAttachKernelConsole(pMxProcess->HostProcess, &hdlConsole));
    AUTO_RESOURCE(hdlConsole, ZwClose);

    if (Input != NULL || Output != NULL)
    {
        MX_RETURN_IF_FAIL(CoOpenStandardHandles(hdlConsole, Input, Output));
    }

    if (Console != NULL)
    {
        *Console = hdlConsole;
        hdlConsole = NULL;
    }

    return STATUS_SUCCESS;
}
