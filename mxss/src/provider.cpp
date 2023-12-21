#include "provider.h"

#include "syscall.h"

PS_PICO_ROUTINES MxRoutines
{
    .Size = sizeof(PS_PICO_ROUTINES)
};

extern "C"
VOID
MxSystemCallDispatch(
    _In_ PPS_PICO_SYSTEM_CALL_INFORMATION SystemCall
)
{
#ifdef AMD64
    INT iSysNum = (INT)SystemCall->TrapFrame->Rax;
    UINT_PTR uArg1 = SystemCall->TrapFrame->Rdi;
    UINT_PTR uArg2 = SystemCall->TrapFrame->Rsi;
    UINT_PTR uArg3 = SystemCall->TrapFrame->Rdx;
    UINT_PTR uArg4 = SystemCall->TrapFrame->R10;
    UINT_PTR uArg5 = SystemCall->TrapFrame->R8;
    UINT_PTR uArg6 = SystemCall->TrapFrame->R9;
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

    // Syscall numbers are defined here:
    // https://github.com/itsmevjnk/sysx/blob/main/exec/syscall.h
    switch (iSysNum)
    {
        case 0: // SYSCALL_EXIT
        {
            iRet = SyscallExit((INT)uArg1);
        }
        break;
        case 1: // SYSCALL_READ
        {
            DbgBreakPoint();
        }
        break;
        case 2: // SYSCALL_WRITE
        {
            iRet = SyscallWrite((INT)uArg1, (PVOID)uArg2, (SIZE_T)uArg3);
        }
        break;
        default:
            KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
                "Unimplemented syscall: %x\n", iSysNum));
    }

#ifdef AMD64
    SystemCall->TrapFrame->Rax = (ULONG64)iRet;
#else
#error Set the syscall return value for this architecture!
#endif
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
