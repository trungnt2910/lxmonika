#include <cstddef>
#include <cstdint>

template <typename... Args>
inline static
intptr_t
MonixSyscall(
    intptr_t number,
    Args... args
)
{
    constexpr size_t argsCount = sizeof...(Args);

    // TODO: Use C++26 pack indexing to prevent the overhead of an extra array
    // when compiled without optimizations. Also avoids hacks like the one below.

    // ISO C++ forbids zero-size array.
    constexpr size_t argsArrSize = argsCount == 0 ? 1 : argsCount;
    intptr_t argsArray[argsArrSize] = { (intptr_t)args... };

    static_assert(argsCount <= 6, "Too many arguments for a Linux syscall.");

#define SET_REGISTER_IF_PRESENT(index)              \
    do                                              \
    {                                               \
        if constexpr (argsCount > index)            \
        {                                           \
            reg_r##index = argsArray[index];        \
        }                                           \
    }                                               \
    while (0)

#define DO_SYSCALL(op, rnum, rret, r0, r1, r2, r3, r4, r5, ...)             \
    do                                                                      \
    {                                                                       \
        /* DON'T intialize these registers, otherwise the compiler */       \
        /* will generate additional code to zero them out! */               \
        register intptr_t reg_rnum asm(#rnum);                              \
        register intptr_t reg_r0   asm(#r0);                                \
        register intptr_t reg_r1   asm(#r1);                                \
        register intptr_t reg_r2   asm(#r2);                                \
        register intptr_t reg_r3   asm(#r3);                                \
        register intptr_t reg_r4   asm(#r4);                                \
        register intptr_t reg_r5   asm(#r5);                                \
        register intptr_t reg_rret asm(#rret);                              \
                                                                            \
        /* The macros below expand to `if constexpr` statements. */         \
        /* The registers will only be attached to existing arguments. */    \
        reg_rnum = number;                                                  \
        SET_REGISTER_IF_PRESENT(0);                                         \
        SET_REGISTER_IF_PRESENT(1);                                         \
        SET_REGISTER_IF_PRESENT(2);                                         \
        SET_REGISTER_IF_PRESENT(3);                                         \
        SET_REGISTER_IF_PRESENT(4);                                         \
        SET_REGISTER_IF_PRESENT(5);                                         \
                                                                            \
        asm volatile(                                                       \
            #op                                                             \
            /* Outputs */                                                   \
            : "+r"(reg_rret)                                                \
            /* Inputs */                                                    \
            : "r"(reg_rnum),                                                \
              "r"(reg_r0), "r"(reg_r1), "r"(reg_r2),                        \
              "r"(reg_r3), "r"(reg_r4), "r"(reg_r5)                         \
            /* Clobbers */                                                  \
            : "memory" __VA_OPT__(,) __VA_ARGS__                            \
        );                                                                  \
                                                                            \
        return reg_rret;                                                    \
    }                                                                       \
    while (0)
#ifdef __x86_64__
    DO_SYSCALL(syscall, rax, rax,
               rdi, rsi, rdx, r10, r8, r9,
               "rcx", "r11");
#elifdef __i386__
    DO_SYSCALL(int $0x80, eax, eax,
               ebx, ecx, edx, esi, edi, ebp);
#elifdef __aarch64__
    DO_SYSCALL(svc #0, x8, x0,
               x0, x1, x2, x3, x4, x5,
               "cc");
#elifdef __arm__
    // This code only supports arm thumb code.
    // Without thumb mode, r7 is reserved as the frame pointer and using it for the syscall may
    // make some compilers complain.
    // See https://github.com/bminor/musl/blob/master/arch/arm/syscall_arch.h for more details.
    // Since Windows on ARM only supports thumb mode
    // (https://learn.microsoft.com/en-us/cpp/build/overview-of-arm-abi-conventions), this should
    // not be a problem.
    DO_SYSCALL(swi #0, r7, r0,
               r0, r1, r2, r3, r4, r5);
#else
#error Write the syscall code for this architecture!
#endif

#undef SET_REGISTER_IF_PRESENT
#undef DO_SYSCALL

}

// https://github.com/itsmevjnk/sysx/blob/main/exec/syscall.h
/* syscall function numbers */
#define SYSCALL_EXIT                            0 // arg1 = return code
#define SYSCALL_READ                            1 // arg1 = size, arg2 = buffer ptr, arg3 = fd
#define SYSCALL_WRITE                           2 // arg1 = size, arg2 = buffer ptr, arg3 = fd
#define SYSCALL_FORK                            3

#define STRING_AND_SIZE(str) (str), (sizeof(str) - 1)

extern "C"
void _start()
{
    MonixSyscall(SYSCALL_WRITE, 1, STRING_AND_SIZE("Hello, userland World!\n"));

    if (MonixSyscall(SYSCALL_FORK) == 0)
    {
        MonixSyscall(SYSCALL_WRITE, 1, STRING_AND_SIZE("(from another, forked, process)\n"));
    }

    MonixSyscall(SYSCALL_EXIT, 0);
}
