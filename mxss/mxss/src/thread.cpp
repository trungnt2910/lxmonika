#include "thread.h"

NTSTATUS
MxThreadAllocate(
    _Out_ PMX_THREAD* pPMxThread
)
{
    PMX_THREAD pMxThread = (PMX_THREAD)
        ExAllocatePoolZero(PagedPool, sizeof(MX_THREAD), '  xM');

    if (pMxThread == NULL)
    {
        return STATUS_NO_MEMORY;
    }

    pMxThread->ReferenceCount = 1;

    *pPMxThread = pMxThread;
    return STATUS_SUCCESS;
}

NTSTATUS
MxThreadReference(
    _Inout_ PMX_THREAD pMxThread
)
{
    ULONG_PTR uNewCount = InterlockedIncrementSizeT(&pMxThread->ReferenceCount);

    UNREFERENCED_PARAMETER(uNewCount);
    ASSERT(uNewCount != 1);

    return STATUS_SUCCESS;
}

VOID
MxThreadFree(
    _Inout_ PMX_THREAD pMxThread
)
{
    ULONG_PTR uNewCount = InterlockedDecrementSizeT(&pMxThread->ReferenceCount);
    ASSERT(uNewCount + 1 > uNewCount);

    if (uNewCount != 0)
    {
        return;
    }

    ExFreePoolWithTag(pMxThread, '  xM');
}
