#pragma once

// syscall.h
//
// Monix system call emulation

#include <ntddk.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Monix is based on SysX, so the syscall numbers should match
// https://github.com/itsmevjnk/sysx/blob/main/exec/syscall.h
#define SYSCALL_EXIT                            0 // arg1 = return code
#define SYSCALL_READ                            1 // arg1 = size, arg2 = buffer ptr, arg3 = fd
#define SYSCALL_WRITE                           2 // arg1 = size, arg2 = buffer ptr, arg3 = fd

INT
    SyscallExit(
        _In_ INT status
    );

INT_PTR
    SyscallWrite(
        _In_ INT fd,
        _In_reads_opt_(size) PVOID buffer,
        _In_ SIZE_T size
    );

#ifdef __cplusplus
}
#endif
