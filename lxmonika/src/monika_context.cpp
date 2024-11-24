#include "monika.h"

#include "Logger.h"

#define MA_CONTEXT_TAG ('xCaM')

extern "C"
PMA_CONTEXT
MapAllocateContext(
    _In_ DWORD Provider,
    _In_opt_ PVOID OriginalContext,
    _In_opt_ PPS_PICO_CREATE_INFO CreateInfo
)
{
    PMA_CONTEXT pContext = (PMA_CONTEXT)
        ExAllocatePool2(POOL_FLAG_PAGED, sizeof(MA_CONTEXT), MA_CONTEXT_TAG);

    if (pContext != NULL)
    {
        *pContext = MA_CONTEXT
        {
            .Magic = MA_CONTEXT_MAGIC,
            .Provider = Provider,
            .Context = OriginalContext,
            .Parent = NULL
        };

        if (CreateInfo != NULL
            && CreateInfo->ImageFileName != NULL
            && CreateInfo->ImageFileName->Length != 0)
        {
            USHORT uLen = CreateInfo->ImageFileName->Length;

            pContext->ImageFileName.Buffer = (PWSTR)
                ExAllocatePool2(POOL_FLAG_PAGED, uLen, MA_CONTEXT_TAG);

            if (pContext->ImageFileName.Buffer != NULL)
            {
                pContext->ImageFileName.MaximumLength = uLen;
                RtlCopyUnicodeString(&pContext->ImageFileName, CreateInfo->ImageFileName);
            }
            else
            {
                Logger::LogWarning("Failed to allocate memory for image file name.");
            }
        }
    }

    return pContext;
}

extern "C"
VOID
MapFreeContext(
    _In_ PMA_CONTEXT Context
)
{
#if DBG
    if (Context == NULL || Context->Magic != MA_CONTEXT_MAGIC)
    {
        DbgBreakPoint();
    }
#endif

    do
    {
        PMA_CONTEXT pParentContext = Context->Parent;
        if (Context->ImageFileName.Buffer != NULL)
        {
            ExFreePoolWithTag(Context->ImageFileName.Buffer, MA_CONTEXT_TAG);
        }
        ExFreePoolWithTag(Context, MA_CONTEXT_TAG);
        Context = pParentContext;
    }
    while (Context != NULL);
}

extern "C"
NTSTATUS
MapPushContext(
    _Inout_ PMA_CONTEXT CurrentContext,
    _In_ PMA_CONTEXT NewContext
)
{
#ifdef DBG
    if (CurrentContext == NULL || CurrentContext->Magic != MA_CONTEXT_MAGIC
        || NewContext == NULL || NewContext->Magic != MA_CONTEXT_MAGIC)
    {
        DbgBreakPoint();
        return STATUS_INVALID_PARAMETER;
    }
#endif

    MA_CONTEXT TempContext = *CurrentContext;
    *CurrentContext = *NewContext;
    CurrentContext->Parent = NewContext;
    *NewContext = TempContext;

    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
MapPopContext(
    _Inout_ PMA_CONTEXT CurrentContext
)
{
#ifdef DBG
    if (CurrentContext == NULL || CurrentContext->Magic != MA_CONTEXT_MAGIC
        || CurrentContext->Parent == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }
#endif

    MA_CONTEXT TempContext = *(CurrentContext->Parent);
    ExFreePoolWithTag(CurrentContext->Parent, MA_CONTEXT_TAG);
    *CurrentContext = TempContext;

    return STATUS_SUCCESS;
}
