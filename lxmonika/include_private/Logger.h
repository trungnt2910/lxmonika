#pragma once

#include <intsafe.h>
#include <ntddk.h>

enum class LogLevel
{
    Trace,
    Info,
    Warning,
    Error
};

#if DBG
#define LOGGER_MINIMUM_LEVEL (LogLevel::Trace)
#else
#define LOGGER_MINIMUM_LEVEL (LogLevel::Warning)
#endif

class Logger
{
private:
    static LONG _Initialized;
    static FAST_MUTEX _Mutex;

    template <typename TFirst, typename... TRest>
    static void _Print(TFirst first, TRest... args)
    {
        _Print(first);
        _Print(args...);
    }

    static void _Print() {}

    static void _Print(bool b);

    static void _Print(WORD w);
    static void _Print(DWORD dw);
    static void _Print(ULONGLONG ull);

    static void _Print(INT i);

    static void _Print(PSTR pStr);
    static void _Print(PCSTR pcStr);
    static void _Print(PUCHAR pcuStr);

    static void _Print(PVOID p);
    template <typename T>
    static void _Print(T* ptr) { _Print((PVOID)ptr); }
    template <typename T>
    static void _Print(const T* ptr) { _Print((PVOID)ptr); }

    static void _Lock();
    static void _Unlock();
public:
    static bool _Log(LogLevel level, const char* file, int line, const char* function);

    template <LogLevel level, typename... TRest>
    static bool _Log(const char* file, int line, const char* function,
        TRest... args)
    {
        if constexpr (level < LOGGER_MINIMUM_LEVEL)
        {
            UNREFERENCED_PARAMETER(file);
            UNREFERENCED_PARAMETER(line);
            UNREFERENCED_PARAMETER(function);
            PVOID unused[] = {&args...};
            UNREFERENCED_PARAMETER(unused);
            return false;
        }
        else
        {
            if (!_Log(level, file, line, function))
            {
                return false;
            }

            _Print(args...);
            _Print("\n");

            _Unlock();
            return true;
        }
    }
};

#define LogTrace(...)           \
    _Log<LogLevel::Trace>(      \
        __FILE__,               \
        __LINE__,               \
        __func__,               \
        __VA_ARGS__             \
    )

#define LogInfo(...)            \
    _Log<LogLevel::Info>(       \
        __FILE__,               \
        __LINE__,               \
        __func__,               \
        __VA_ARGS__             \
    )

#define LogWarning(...)         \
    _Log<LogLevel::Warning>(    \
        __FILE__,               \
        __LINE__,               \
        __func__,               \
        __VA_ARGS__             \
    )

#define LogError(...)           \
    _Log<LogLevel::Error>(      \
        __FILE__,               \
        __LINE__,               \
        __func__,               \
        __VA_ARGS__             \
    )
