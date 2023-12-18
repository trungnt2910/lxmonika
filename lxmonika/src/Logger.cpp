#include "Logger.h"

#include <ntstrsafe.h>

LONG Logger::_Initialized = 0;
FAST_MUTEX Logger::_Mutex;

#define WRITE(format, ...)								\
    DbgPrintEx(											\
        /* Just do what everyone does. */               \
        DPFLTR_IHVDRIVER_ID,                            \
        /* Prevent it from being filtered in WinDbg */  \
        DPFLTR_ERROR_LEVEL,                             \
        format,                                         \
        __VA_ARGS__                                     \
    );

bool
Logger::_Log(LogLevel level, const char* file, int line, const char* function)
{
    if (level < LOGGER_MINIMUM_LEVEL)
    {
        return false;
    }

    // TODO: File name check.

    _Lock();

    const char* levelStr = "???";

    switch (level)
    {
        case LogLevel::Trace:
            levelStr = "TRC";
        break;
        case LogLevel::Info:
            levelStr = "INF";
        break;
        case LogLevel::Warning:
            levelStr = "WRN";
        break;
        case LogLevel::Error:
            levelStr = "ERR";
        break;
    }

    WRITE("%s ", levelStr);

    // Extract name from file path.
    size_t pathLength = strlen(file);
    int lastComponent = (int)pathLength;
    while (lastComponent >= 0 && file[lastComponent] != '\\')
    {
        --lastComponent;
    }
    file = file + lastComponent + 1;

    const int COLUMN_WIDTH = 32;

    char buffer[COLUMN_WIDTH + 1];
    int len = _snprintf(buffer, COLUMN_WIDTH, "%s:%i", file, line);
    for (int i = len; i < COLUMN_WIDTH; ++i)
    {
        buffer[i] = ' ';
    }
    buffer[COLUMN_WIDTH] = '\0';
    WRITE("%s", buffer);

    len = _snprintf(buffer, COLUMN_WIDTH, "%s", function);
    for (int i = len; i < COLUMN_WIDTH; ++i)
    {
        buffer[i] = ' ';
    }
    buffer[COLUMN_WIDTH] = '\0';
    WRITE("%s", buffer);

    return true;
}

void
Logger::_Lock()
{
    if (InterlockedCompareExchange(&_Initialized, 1, 0))
    {
        LARGE_INTEGER lInt
        {
            .QuadPart = -100000
        };
        while (_Initialized == 1)
        {
            KeDelayExecutionThread(KernelMode, FALSE, &lInt);
        }
    }
    else
    {
        // Won the race.
        ExInitializeFastMutex(&_Mutex);
        _Initialized = 2;
    }

    ExAcquireFastMutex(&_Mutex);
}

void
Logger::_Unlock()
{
    ExReleaseFastMutex(&_Mutex);
}

void
Logger::_Print(bool b)
{
    WRITE("%s", b ? "true" : "false");
}

void
Logger::_Print(WORD w)
{
    WRITE("%hu", w);
}

void
Logger::_Print(DWORD dw)
{
    WRITE("%u", dw);
}

void
Logger::_Print(ULONGLONG ull)
{
    WRITE("%llu", ull);
}

void
Logger::_Print(INT i)
{
    WRITE("%i", i);
}

void
Logger::_Print(PSTR pStr)
{
    WRITE("%s", pStr);
}

void
Logger::_Print(PCSTR pcStr)
{
    WRITE("%s", pcStr);
}

void
Logger::_Print(PUCHAR pcuStr)
{
    WRITE("%s", (const char*)pcuStr);
}

void
Logger::_Print(PVOID p)
{
    WRITE("%p", p);
}
