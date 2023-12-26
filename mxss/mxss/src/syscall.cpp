#include "syscall.h"

#include <intsafe.h>

#include "console.h"
#include "process.h"
#include "provider.h"

extern "C"
INT
SyscallExit(
    _In_ INT status
)
{
    PEPROCESS pCurrentProcess = PsGetCurrentProcess();

    PMX_PROCESS pContext = (PMX_PROCESS)MxRoutines.GetProcessContext(pCurrentProcess);
    if (pContext != NULL)
    {
        // TODO: Do some conversion to/from NTSTATUS values?
        pContext->ExitStatus = status;

        // We do not need a context attached to our process anymore.
        MxProcessFree(pContext);
        pContext = NULL;
    }

    // Actually not suicide, we will still return.
    MxRoutines.TerminateProcess(pCurrentProcess, status);

    // Currently Monix processes are single-threaded.
    PETHREAD pCurrentThread = PsGetCurrentThread();
    // Microsoft also uses TRUE, so why don't we?
    MxRoutines.TerminateThread(pCurrentThread, status, TRUE);

    // For threads that we did not create (those assigned to us by lxmonika), the call above will
    // actually return. TerminateThread will not kill the thread, but unassigns us as the provider.
    // When we return here, control will be passed to the previous provider (usually WSL).
    return 0;
}

extern "C"
INT_PTR
SyscallWrite(
    _In_ INT fd,
    _In_reads_opt_(size) PVOID buffer,
    _In_ SIZE_T size
)
{
    // Yes, this is our "FD table".
    if (fd != 1 && fd != 2)
    {
        return -1;
    }

    if (buffer == NULL)
    {
        return -1;
    }

    INT_PTR returnValue = 0;
    SIZE_T written = 0;

    HANDLE hdlConsole = NULL;
    HANDLE hdlOutput = NULL;

    const auto WriteToEnd = [&](PCHAR pBuffer, PULONG uCount)
    {
        NTSTATUS status = STATUS_SUCCESS;
        IO_STATUS_BLOCK ioStatus;

        ULONG uWritten = 0;

        while (uWritten < *uCount)
        {
            ioStatus.Information = 0;

            status = ZwWriteFile(
                hdlOutput,
                NULL,
                NULL,
                NULL,
                &ioStatus,
                pBuffer + uWritten,
                *uCount - uWritten,
                NULL,
                NULL
            );

            if (!NT_SUCCESS(status))
            {
                break;
            }

            uWritten += (ULONG)ioStatus.Information;
        }

        *uCount = uWritten;
        return status;
    };

    PEPROCESS pCurrentProcess = PsGetCurrentProcess();

    PMX_PROCESS pContext = (PMX_PROCESS)MxRoutines.GetProcessContext(pCurrentProcess);

    if (pContext == NULL)
    {
        // Some random process assigned to us by lxmonika. Write not supported.
        // In the future, there are plans to unify console handle management.
        returnValue = -1;
        goto end;
    }

    NTSTATUS status;

    status = CoAttachKernelConsole(PsGetProcessId(pContext->HostProcess), &hdlConsole);

    if (!NT_SUCCESS(status))
    {
        returnValue = -1;
        goto end;
    }

    status = CoOpenStandardHandles(hdlConsole, NULL, &hdlOutput);

    if (!NT_SUCCESS(status))
    {
        returnValue = -1;
        goto end;
    }

    while (written < size)
    {
        __try
        {
            SIZE_T uMaxWrite = min(size - written, ULONG_MAX);

            PCHAR pCurrentBuffer = (PCHAR)buffer + written;
            PCHAR pNextNewLine = (PCHAR)memchr(pCurrentBuffer, '\n', uMaxWrite);

            if (pNextNewLine != NULL)
            {
                uMaxWrite = min(uMaxWrite, (SIZE_T)(pNextNewLine - pCurrentBuffer));
            }

            if (uMaxWrite > 0)
            {
                ULONG uCurrentWritten = (ULONG)uMaxWrite;
                status = WriteToEnd(pCurrentBuffer, &uCurrentWritten);

                written += uCurrentWritten;

                if (!NT_SUCCESS(status))
                {
                    returnValue = -1;
                    goto end;
                }
            }

            if (pNextNewLine != NULL)
            {
                CHAR pWinNewLine[] = "\r\n";
                ULONG count = sizeof(pWinNewLine) - 1;
                status = WriteToEnd(pWinNewLine, &count);

                if (!NT_SUCCESS(status))
                {
                    returnValue = -1;
                    goto end;
                }

                ++written;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            returnValue = -1;
            goto end;
        }
    }
end:
    if (written != 0)
    {
        returnValue = written;
    }

    if (hdlOutput != NULL)
    {
        ZwClose(hdlOutput);
    }

    if (hdlConsole != NULL)
    {
        ZwClose(hdlConsole);
    }

    return returnValue;
}
