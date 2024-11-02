#include "process.h"

#include "elf.h"
#include "os.h"
#include "provider.h"
#include "thread.h"

#include "AutoResource.h"

#ifdef _WIN64
#define ElfW(type) Elf64_##type
#elif defined(_WIN32)
#define ElfW(type) Elf32_##type
#endif

#define MX_RETURN_IF_FAIL(s)        \
    do                              \
    {                               \
        NTSTATUS status__ = (s);    \
        if (!NT_SUCCESS(status__))  \
            return status__;        \
    }                               \
    while (FALSE)

#define MX_POOL_TAG ('  xM')

// An unofficial, more descriptive name for the flag.
// This undocumented flag used to be there to allow compatibility with 16-bit applications only on
// 32-bit x86 Windows.
// It forces pages to be allocated with a granularity equal to the processor page boundary (4KB).
// Recently, the flag has been enabled on all architectures, but only for Pico processes, hence the
// unofficial name.
#define MX_MEM_PICO MEM_DOS_LIM

extern "C"
NTSTATUS
MxProcessExecute(
    _In_ PUNICODE_STRING pExecutablePath,
    _In_ PEPROCESS pParentProcess,
    _In_ PEPROCESS pHostProcess,
    _In_opt_ HANDLE hdlCwd,
    _Out_ PMX_PROCESS* pPMxProcess
)
{
    PMX_PROCESS pMxProcess = (PMX_PROCESS)
        ExAllocatePoolZero(PagedPool, sizeof(MX_PROCESS), MX_POOL_TAG);
    if (pMxProcess == NULL)
    {
        return STATUS_NO_MEMORY;
    }
    AUTO_RESOURCE(pMxProcess, MxProcessFree);
    pMxProcess->ReferenceCount = 1;

    OBJECT_ATTRIBUTES objAttributes;
    InitializeObjectAttributes(
        &objAttributes,
        pExecutablePath,
        OBJ_CASE_INSENSITIVE,
        hdlCwd,
        NULL
    );

    IO_STATUS_BLOCK ioStatus;

    HANDLE hdlExecutable = NULL;
    MX_RETURN_IF_FAIL(ZwOpenFile(
        &hdlExecutable,
        FILE_GENERIC_READ | FILE_GENERIC_EXECUTE,
        &objAttributes,
        &ioStatus,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_SYNCHRONOUS_IO_NONALERT
    ));
    AUTO_RESOURCE(hdlExecutable, ZwClose);

    MX_RETURN_IF_FAIL(ObReferenceObjectByHandle(
        hdlExecutable,
        FILE_GENERIC_READ | FILE_GENERIC_EXECUTE,
        *IoFileObjectType,
        KernelMode,
        (PVOID*)&pMxProcess->MainExecutable,
        NULL
    ));

    HANDLE hdlParentProcess = NULL;
    MX_RETURN_IF_FAIL(ObOpenObjectByPointer(
        pParentProcess,
        OBJ_KERNEL_HANDLE,
        NULL,
        PROCESS_ALL_ACCESS,
        *PsProcessType,
        KernelMode,
        &hdlParentProcess
    ));
    AUTO_RESOURCE(hdlParentProcess, ZwClose);

    PS_PICO_PROCESS_ATTRIBUTES psPicoProcessAttributes
    {
        .ParentProcess = hdlParentProcess,
        .Context = pMxProcess
    };

    PS_PICO_CREATE_INFO psPicoCreateInfo
    {
        .FileObject = pMxProcess->MainExecutable,
        .ImageFileName = pExecutablePath
    };

    HANDLE hdlProcess = NULL;
    MX_RETURN_IF_FAIL(MxRoutines.CreateProcess(&psPicoProcessAttributes,
        &psPicoCreateInfo, &hdlProcess));
    AUTO_RESOURCE(hdlProcess, ZwClose);

    PEPROCESS pProcess = NULL;
    MX_RETURN_IF_FAIL(ObReferenceObjectByHandle(
        hdlProcess,
        PROCESS_ALL_ACCESS,
        *PsProcessType,
        KernelMode,
        (PVOID*)&pProcess,
        NULL
    ));
    AUTO_RESOURCE(pProcess, [](auto pProcess)
    {
        MxRoutines.TerminateProcess(pProcess, STATUS_UNSUCCESSFUL);
        ObDereferenceObject(pProcess);
    });

    PVOID pCodeBaseAddress = NULL;

    // "Borrow" a pProcess reference for a while.
    pMxProcess->Process = pProcess;
    NTSTATUS status = MxProcessMapMainExecutable(pMxProcess, &pCodeBaseAddress);
    // Prevent the cleanup function to free a second time.
    pMxProcess->Process = NULL;

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    PVOID pStackBaseAddress = NULL;
    SIZE_T szStackSize = 0x800000;
    LARGE_INTEGER liStackSize
    {
        .QuadPart = (LONGLONG)szStackSize
    };
    HANDLE hdlStackSection = NULL;
    MX_RETURN_IF_FAIL(ZwCreateSection(
        &hdlStackSection,
        STANDARD_RIGHTS_REQUIRED | SECTION_MAP_READ | SECTION_QUERY | SECTION_EXTEND_SIZE,
        NULL,
        &liStackSize,
        PAGE_WRITECOPY,
        SEC_COMMIT,
        NULL
    ));
    AUTO_RESOURCE(hdlStackSection, ZwClose);
    MX_RETURN_IF_FAIL(ZwMapViewOfSection(
        hdlStackSection,
        hdlProcess,
        &pStackBaseAddress,
        0,
        szStackSize,
        NULL,
        &szStackSize,
        ViewShare,
        MX_MEM_PICO,
        PAGE_WRITECOPY
    ));

    PMX_THREAD pMxThread = NULL;
    MX_RETURN_IF_FAIL(MxThreadAllocate(&pMxThread));
    AUTO_RESOURCE(pMxThread, MxThreadFree);

    PS_PICO_THREAD_ATTRIBUTES psPicoThreadAttributes
    {
        .Process = hdlProcess,
        .UserStack = (ULONG_PTR)pStackBaseAddress + szStackSize,
        .StartRoutine = (ULONG_PTR)pCodeBaseAddress,
        .Context = pMxThread
    };

    HANDLE hdlThread = NULL;
    MX_RETURN_IF_FAIL(MxRoutines.CreateThread(&psPicoThreadAttributes,
        &psPicoCreateInfo, &hdlThread));
    AUTO_RESOURCE(hdlThread, ZwClose);

    // One more reference that NT holds.
    MxThreadReference(pMxThread);

    PETHREAD pThread = NULL;
    MX_RETURN_IF_FAIL(ObReferenceObjectByHandle(
        hdlThread,
        THREAD_ALL_ACCESS,
        *PsThreadType,
        KernelMode,
        (PVOID*)&pThread,
        NULL
    ));
    AUTO_RESOURCE(pThread, [](auto pThread)
    {
        MxThreadFree((PMX_THREAD)MxRoutines.GetThreadContext(pThread));
        MxRoutines.TerminateThread(pThread, STATUS_UNSUCCESSFUL, TRUE);
        ObDereferenceObject(pThread);
    });

    ObReferenceObject(pHostProcess);
    pMxProcess->HostProcess = pHostProcess;

    // One more reference for the output. The other reference is given to NT.
    InterlockedIncrementSizeT(&pMxProcess->ReferenceCount);

    pMxProcess->UserStack = pStackBaseAddress;

    // These don't need to be dereferenced/terminated anymore.
    pMxProcess->Process = pProcess;
    pProcess = NULL;
    pMxProcess->Thread = pThread;
    pThread = NULL;
    pMxProcess->MxThread = pMxThread;
    pMxThread = NULL;

    // We have to properly set the context before allowing execution.
    MxRoutines.ResumeThread(pMxProcess->Thread, NULL);

    *pPMxProcess = pMxProcess;
    pMxProcess = NULL;

    return STATUS_SUCCESS;
}

extern "C"
VOID
MxProcessFree(
    _In_ PMX_PROCESS pMxProcess
)
{
    ULONG_PTR uNewRefCount = InterlockedDecrementSizeT(&pMxProcess->ReferenceCount);

    // Assert not overflowing.
    ASSERT(uNewRefCount + 1 > uNewRefCount);

    if (uNewRefCount != 0)
    {
        return;
    }

    if (pMxProcess->Thread)
    {
        ObDereferenceObject(pMxProcess->Thread);
    }

    if (pMxProcess->Process)
    {
        ObDereferenceObject(pMxProcess->Process);
    }

    if (pMxProcess->MainExecutable)
    {
        ObDereferenceObject(pMxProcess->MainExecutable);
    }

    if (pMxProcess->HostProcess)
    {
        ObDereferenceObject(pMxProcess->HostProcess);
    }

    if (pMxProcess->MxThread)
    {
        MxThreadFree(pMxProcess->MxThread);
    }

    ExFreePoolWithTag(pMxProcess, MX_POOL_TAG);
}

extern "C"
NTSTATUS
MxProcessMapMainExecutable(
    _In_ PMX_PROCESS pMxProcess,
    _Out_ PVOID* pEntryPoint
)
{
    if (pMxProcess == NULL || pMxProcess->Process == NULL || pMxProcess->MainExecutable == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    HANDLE hdlMainExecutable = NULL;
    MX_RETURN_IF_FAIL(ObOpenObjectByPointer(
        pMxProcess->MainExecutable,
        OBJ_KERNEL_HANDLE,
        NULL,
        FILE_ALL_ACCESS,
        *IoFileObjectType,
        KernelMode,
        &hdlMainExecutable
    ));
    AUTO_RESOURCE(hdlMainExecutable, ZwClose);

    const auto ReadToEnd = [&](PVOID pBuffer, SIZE_T uOffset, SIZE_T uSize)
    {
        IO_STATUS_BLOCK ioStatus;
        SIZE_T uRead = 0;
        LARGE_INTEGER liOffset;
        while (uRead < uSize)
        {
            liOffset.QuadPart = uOffset + uRead;
            MX_RETURN_IF_FAIL(ZwReadFile(
                hdlMainExecutable,
                NULL,
                NULL,
                NULL,
                &ioStatus,
                (PCHAR)pBuffer + uRead,
                (ULONG)(uSize - uRead),
                &liOffset,
                NULL
            ));
            uRead += ioStatus.Information;
        }
        return STATUS_SUCCESS;
    };

    ElfW(Ehdr) elfHeader;
    MX_RETURN_IF_FAIL(ReadToEnd(&elfHeader, 0, sizeof(ElfW(Ehdr))));

    if (!IS_ELF(elfHeader)
        || (elfHeader.e_phentsize != sizeof(ElfW(Phdr)))
        || elfHeader.e_phnum < 1
        || elfHeader.e_type != ET_EXEC)
    {
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    SIZE_T szPhdrsCount = elfHeader.e_phnum;
    SIZE_T szPhdrsBytes = sizeof(ElfW(Phdr)) * szPhdrsCount;

    ElfW(Phdr)* pProgramHeaders = (ElfW(Phdr)*)
        ExAllocatePoolZero(PagedPool, szPhdrsBytes, MX_POOL_TAG);
    AUTO_RESOURCE(pProgramHeaders, [](auto p) { ExFreePoolWithTag(p, MX_POOL_TAG); });
    MX_RETURN_IF_FAIL(ReadToEnd(pProgramHeaders, elfHeader.e_phoff, szPhdrsBytes));

    ElfW(Addr) pImageStartVm = (ElfW(Addr))-1;
    ElfW(Addr) pImageEndVm = 0;

    for (SIZE_T i = 0; i < szPhdrsCount; ++i)
    {
        if (pProgramHeaders[i].p_type != PT_LOAD)
        {
            continue;
        }

        if ((ALIGN_DOWN_BY(pProgramHeaders[i].p_offset, PAGE_SIZE)
                != pProgramHeaders[i].p_offset)
            && (ALIGN_DOWN_BY(pProgramHeaders[i].p_vaddr, PAGE_SIZE)
                != pProgramHeaders[i].p_vaddr))
        {
            return STATUS_INVALID_IMAGE_FORMAT;
        }

        // TODO: Handle the alignment.

        pImageStartVm = min(pImageStartVm, pProgramHeaders[i].p_vaddr);
        pImageEndVm = max(pImageEndVm, pProgramHeaders[i].p_vaddr + pProgramHeaders[i].p_memsz);
    }

    // TODO: Reserve a block of memory.
    ElfW(Addr) pImageBase = 0;

    HANDLE hdlSection = NULL;
    MX_RETURN_IF_FAIL(ZwCreateSection(
        &hdlSection,
        STANDARD_RIGHTS_REQUIRED
            | SECTION_MAP_EXECUTE | SECTION_MAP_READ | SECTION_QUERY | SECTION_EXTEND_SIZE,
        NULL,
        NULL,
        PAGE_EXECUTE_WRITECOPY,
        SEC_COMMIT,
        hdlMainExecutable
    ));
    AUTO_RESOURCE(hdlSection, ZwClose);

    HANDLE hdlProcess = NULL;
    MX_RETURN_IF_FAIL(ObOpenObjectByPointer(
        pMxProcess->Process,
        OBJ_KERNEL_HANDLE,
        NULL,
        PROCESS_ALL_ACCESS,
        *PsProcessType,
        KernelMode,
        &hdlProcess
    ));
    AUTO_RESOURCE(hdlProcess, ZwClose);

    const auto ElfProtectionToWindows = [](ElfW(Word) elfFlags)
    {
        static const int table[2][2][2] =
        {
            // Not executable
            {
                // Not writable
                { PAGE_NOACCESS, PAGE_READONLY },
                // Writable
                { PAGE_WRITECOPY, PAGE_WRITECOPY }
            },
            // Executable
            {
                // Not writable
                { PAGE_EXECUTE, PAGE_EXECUTE_READ },
                // Writable
                { PAGE_EXECUTE_WRITECOPY, PAGE_EXECUTE_WRITECOPY }
            }
        };

        return table[(bool)(elfFlags & PF_X)][(bool)(elfFlags & PF_W)][(bool)(elfFlags & PF_R)];
    };

    for (SIZE_T i = 0; i < szPhdrsCount; ++i)
    {
        if (pProgramHeaders[i].p_type != PT_LOAD)
        {
            continue;
        }

        // TODO: Handle the alignment.

        ElfW(Addr) pStartOffset = pProgramHeaders[i].p_offset;
        ElfW(Addr) pFileSize = pProgramHeaders[i].p_filesz;
        ElfW(Addr) pStartVm = pImageBase + pProgramHeaders[i].p_vaddr;
        ElfW(Addr) pMemorySize = ALIGN_UP_BY(pProgramHeaders[i].p_memsz, PAGE_SIZE);

        LARGE_INTEGER liSectionOffset
        {
            .QuadPart = (LONGLONG)pStartOffset
        };
        PVOID pMapBase = (PVOID)pStartVm;
        SIZE_T szViewSize = (SIZE_T)pFileSize;

        MX_RETURN_IF_FAIL(ZwMapViewOfSection(
            hdlSection,
            hdlProcess,
            &pMapBase,
            0,
            szViewSize,
            &liSectionOffset,
            &szViewSize,
            ViewShare,
            MX_MEM_PICO,
            PAGE_EXECUTE_WRITECOPY
        ));

        if (pMapBase != (PVOID)pStartVm)
        {
            return STATUS_CONFLICTING_ADDRESSES;
        }

        // Map the rest as an anonymous section.
        if (szViewSize < pMemorySize)
        {
            SIZE_T szBlankViewSize = pMemorySize - szViewSize;

            LARGE_INTEGER liBlankSectionSize
            {
                .QuadPart = (LONGLONG)szBlankViewSize
            };

            HANDLE hdlBlankSection = NULL;
            MX_RETURN_IF_FAIL(ZwCreateSection(
                &hdlBlankSection,
                STANDARD_RIGHTS_REQUIRED
                    | SECTION_MAP_EXECUTE | SECTION_MAP_READ | SECTION_QUERY | SECTION_EXTEND_SIZE,
                NULL,
                &liBlankSectionSize,
                PAGE_EXECUTE_WRITECOPY,
                SEC_COMMIT,
                NULL
            ));
            AUTO_RESOURCE(hdlBlankSection, ZwClose);

            LARGE_INTEGER liBlankSectionOffset
            {
                .QuadPart = 0
            };
            PVOID pBlankMapBase = (PVOID)(pStartVm + szViewSize);

            MX_RETURN_IF_FAIL(ZwMapViewOfSection(
                hdlBlankSection,
                hdlProcess,
                &pBlankMapBase,
                0,
                szBlankViewSize,
                &liBlankSectionOffset,
                &szBlankViewSize,
                ViewShare,
                MX_MEM_PICO,
                PAGE_EXECUTE_WRITECOPY
            ));

            if ((pBlankMapBase != (PVOID)(pStartVm + szViewSize))
                || (szBlankViewSize != pMemorySize - szViewSize))
            {
                return STATUS_CONFLICTING_ADDRESSES;
            }
        }

        SIZE_T uNumberOfBytesToProtect = pMemorySize;

        MX_RETURN_IF_FAIL(ZwProtectVirtualMemory(
            hdlProcess,
            &pMapBase,
            &uNumberOfBytesToProtect,
            ElfProtectionToWindows(pProgramHeaders[i].p_flags),
            NULL
        ));

        if (uNumberOfBytesToProtect != pMemorySize)
        {
            return STATUS_CONFLICTING_ADDRESSES;
        }
    }

    *pEntryPoint = (PVOID)(pImageBase + elfHeader.e_entry);
    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
MxProcessFork(
    _In_ PMX_PROCESS pMxParentProcess,
    _Out_ PMX_PROCESS* pPMxProcess
)
{
    PMX_PROCESS pMxProcess = (PMX_PROCESS)
        ExAllocatePoolZero(PagedPool, sizeof(MX_PROCESS), MX_POOL_TAG);
    if (pMxProcess == NULL)
    {
        return STATUS_NO_MEMORY;
    }
    AUTO_RESOURCE(pMxProcess, MxProcessFree);
    pMxProcess->ReferenceCount = 1;

    ULONG uNameLen = 0;
    ObQueryNameString(pMxParentProcess->MainExecutable, NULL, 0, &uNameLen);

    POBJECT_NAME_INFORMATION pObNameInfo = (POBJECT_NAME_INFORMATION)
        ExAllocatePoolZero(PagedPool, uNameLen, MX_POOL_TAG);
    if (pObNameInfo == NULL)
    {
        return STATUS_NO_MEMORY;
    }
    AUTO_RESOURCE(pObNameInfo, [](auto p) { ExFreePoolWithTag(p, MX_POOL_TAG); });
    MX_RETURN_IF_FAIL(ObQueryNameString(
        pMxParentProcess->MainExecutable, pObNameInfo, uNameLen, &uNameLen));

    HANDLE hdlParentProcess = NULL;
    MX_RETURN_IF_FAIL(ObOpenObjectByPointer(
        pMxParentProcess->Process,
        OBJ_KERNEL_HANDLE,
        NULL,
        PROCESS_ALL_ACCESS,
        *PsProcessType,
        KernelMode,
        &hdlParentProcess
    ));
    AUTO_RESOURCE(hdlParentProcess, ZwClose);

    PS_PICO_PROCESS_ATTRIBUTES psPicoProcessAttributes
    {
        .ParentProcess = hdlParentProcess,
        .Context = pMxProcess,
        .Flags = PS_PICO_CREATE_PROCESS_CLONE_PARENT | PS_PICO_CREATE_PROCESS_INHERIT_HANDLES
    };

    PS_PICO_CREATE_INFO psPicoCreateInfo
    {
        .FileObject = pMxParentProcess->MainExecutable,
        .ImageFileName = &pObNameInfo->Name
    };

    HANDLE hdlProcess = NULL;
    MX_RETURN_IF_FAIL(MxRoutines.CreateProcess(
        &psPicoProcessAttributes, &psPicoCreateInfo, &hdlProcess));
    AUTO_RESOURCE(hdlProcess, ZwClose);

    PEPROCESS pProcess = NULL;
    MX_RETURN_IF_FAIL(ObReferenceObjectByHandle(
        hdlProcess,
        PROCESS_ALL_ACCESS,
        *PsProcessType,
        KernelMode,
        (PVOID*)&pProcess,
        NULL
    ));
    AUTO_RESOURCE(pProcess, [](auto pProcess)
    {
        MxRoutines.TerminateProcess(pProcess, STATUS_UNSUCCESSFUL);
        ObDereferenceObject(pProcess);
    });

    CONTEXT ctxParent
    {
        .ContextFlags = CONTEXT_ALL
    };
    MX_RETURN_IF_FAIL(MxRoutines.GetContextThreadInternal(
        pMxParentProcess->Thread,
        &ctxParent,
        KernelMode,
        UserMode,
        FALSE
    ));

    PMX_THREAD pMxThread = NULL;
    MX_RETURN_IF_FAIL(MxThreadAllocate(&pMxThread));
    AUTO_RESOURCE(pMxThread, MxThreadFree);

    PS_PICO_THREAD_ATTRIBUTES psPicoThreadAttributes
    {
        .Process = hdlProcess,
        .UserStack = (ULONG_PTR)pMxParentProcess->UserStack,
#ifdef _M_AMD64
        .StartRoutine = (ULONG_PTR)ctxParent.Rip,
#elif defined(_M_ARM64)
        .StartRoutine = (ULONG_PTR)ctxParent.Pc,
#else
#error Copy the instruction pointer for this architecture!
#endif
        .Context = pMxThread,
    };

    HANDLE hdlThread = NULL;
    MX_RETURN_IF_FAIL(MxRoutines.CreateThread(&psPicoThreadAttributes,
        &psPicoCreateInfo, &hdlThread));
    AUTO_RESOURCE(hdlThread, ZwClose);

    MxThreadReference(pMxThread);

    PETHREAD pThread = NULL;
    MX_RETURN_IF_FAIL(ObReferenceObjectByHandle(
        hdlThread,
        THREAD_ALL_ACCESS,
        *PsThreadType,
        KernelMode,
        (PVOID*)&pThread,
        NULL
    ));
    AUTO_RESOURCE(pThread, [](auto pThread)
    {
        MxThreadFree((PMX_THREAD)MxRoutines.GetThreadContext(pThread));
        MxRoutines.TerminateThread(pThread, STATUS_UNSUCCESSFUL, TRUE);
        ObDereferenceObject(pThread);
    });

#ifdef _M_AMD64
    // Syscall return result.
    ctxParent.Rax = 0;

    if (pMxParentProcess->MxThread->CurrentSystemCall != NULL)
    {
        // Restore the "real" context from system call information.
        PKTRAP_FRAME pTrapFrame = pMxParentProcess->MxThread->CurrentSystemCall->TrapFrame;
        ctxParent.Rcx = pTrapFrame->Rcx;
        ctxParent.Rdx = pTrapFrame->Rdx;
        ctxParent.Rbx = pTrapFrame->Rbx;
        ctxParent.Rbp = pTrapFrame->Rbp;
        ctxParent.Rsi = pTrapFrame->Rsi;
        ctxParent.Rdi = pTrapFrame->Rdi;
        ctxParent.R8 = pTrapFrame->R8;
        ctxParent.R9 = pTrapFrame->R9;
        ctxParent.R10 = pTrapFrame->R10;
        ctxParent.R11 = pTrapFrame->R11;
        ctxParent.EFlags = pTrapFrame->EFlags;
    }
#elif defined(_M_ARM64)
    // Syscall return result.
    ctxParent.X0 = 0;

    if (pMxParentProcess->MxThread->CurrentSystemCall != NULL)
    {
        // Restore the "real" context from system call information.
        PKTRAP_FRAME pTrapFrame = pMxParentProcess->MxThread->CurrentSystemCall->TrapFrame;
        ctxParent.X1 = pTrapFrame->X1;
        ctxParent.X2 = pTrapFrame->X2;
        ctxParent.X3 = pTrapFrame->X3;
        ctxParent.X4 = pTrapFrame->X4;
        ctxParent.X5 = pTrapFrame->X5;
        ctxParent.X6 = pTrapFrame->X6;
        ctxParent.X7 = pTrapFrame->X7;
        ctxParent.X8 = pTrapFrame->X8;
        ctxParent.X9 = pTrapFrame->X9;
        ctxParent.X10 = pTrapFrame->X10;
        ctxParent.X11 = pTrapFrame->X11;
        ctxParent.X12 = pTrapFrame->X12;
        ctxParent.X13 = pTrapFrame->X13;
        ctxParent.X14 = pTrapFrame->X14;
        ctxParent.X15 = pTrapFrame->X15;
        ctxParent.X16 = pTrapFrame->X16;
        ctxParent.X17 = pTrapFrame->X17;
        ctxParent.X18 = pTrapFrame->X18;
        ctxParent.Lr = pTrapFrame->Lr;
        ctxParent.Fp = pTrapFrame->Fp;
        ctxParent.Pc = pTrapFrame->Pc;
        ctxParent.Sp = pTrapFrame->Sp;
        memcpy(&ctxParent.Bcr, &pTrapFrame->Bcr, sizeof(ctxParent.Bcr));
        memcpy(&ctxParent.Bvr, &pTrapFrame->Bvr, sizeof(ctxParent.Bvr));
        memcpy(&ctxParent.Wcr, &pTrapFrame->Wcr, sizeof(ctxParent.Wcr));
        memcpy(&ctxParent.Wvr, &pTrapFrame->Wvr, sizeof(ctxParent.Wvr));
    }
#else
#error Set the context for this architecture!
#endif

    MX_RETURN_IF_FAIL(MxRoutines.SetContextThreadInternal(
        pThread,
        &ctxParent,
        KernelMode,
        UserMode,
        FALSE
    ));

    InterlockedIncrementSizeT(&pMxProcess->ReferenceCount);

    pMxProcess->Process = pProcess;
    pProcess = NULL;
    pMxProcess->Thread = pThread;
    pThread = NULL;
    pMxProcess->MxThread = pMxThread;
    pMxThread = NULL;

    ObReferenceObject(pMxParentProcess->HostProcess);
    pMxProcess->HostProcess = pMxParentProcess->HostProcess;

    ObReferenceObject(pMxParentProcess->MainExecutable);
    pMxProcess->MainExecutable = pMxParentProcess->MainExecutable;

    pMxProcess->UserStack = pMxParentProcess->UserStack;

    // We have to properly set the context before allowing execution.
    MxRoutines.ResumeThread(pMxProcess->Thread, NULL);

    *pPMxProcess = pMxProcess;
    pMxProcess = NULL;

    return STATUS_SUCCESS;
}
