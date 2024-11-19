#include "Logger.h"

#include "compat.h"

#include <ntstrsafe.h>
#include <TraceLoggingProvider.h>
#include <winmeta.h>

#include "AutoResource.h"

#define LOGGER_COLUMN_WIDTH 32
#define LOGGER_BUFFER_SIZE 512
#define LOGGER_POOL_TAG 'gLaM'

TRACELOGGING_DEFINE_PROVIDER(
    LgTraceLoggingProvider,
    MONIKA_BUILD_NAME,
    (0x4D6F6E69, 0x6B61, 0x4029, 0x10, 0x52, 0x65, 0x61, 0x6C, 0x69, 0x74, 0x79)
);

static LONG LgInitialized = 0;
static FAST_MUTEX LgMutex;

static WCHAR LgOutputBufferStatic[LOGGER_BUFFER_SIZE];
static SIZE_T LgOutputBufferSize = sizeof(LgOutputBufferStatic);
static PWCHAR LgOutputBuffer = LgOutputBufferStatic;
static PWCHAR LgOutputBufferCurrent = LgOutputBufferStatic;
#define LgOutputBufferOffset ((LgOutputBufferCurrent - LgOutputBuffer) * sizeof(WCHAR))
#define LgOutputBufferSizeLeft (LgOutputBufferSize - LgOutputBufferOffset)

static LogLevel LgCurrentLogLevel;
static PCSTR LgCurrentFile;
static DWORD LgCurrentLine;
static PCSTR LgCurrentFunction;

template <typename T>
void Write(PCWSTR pFormat, T arg)
{
    while (TRUE)
    {
        SIZE_T szCharLeft = LgOutputBufferSizeLeft / sizeof(WCHAR);
        SIZE_T szCharToWrite = _snwprintf(LgOutputBufferCurrent, szCharLeft, pFormat, arg);

        if (szCharToWrite > szCharLeft)
        {
            // Keep this since we'll be messing with LgOutputBuffer[Current] soon.
            SIZE_T szBufferOldOffset = LgOutputBufferOffset;

            SIZE_T szBufferNewSize = LgOutputBufferSize * 2;
            PWCHAR pBufferNew = (PWCHAR)ExAllocatePool2(
                POOL_FLAG_PAGED, szBufferNewSize, LOGGER_POOL_TAG
            );
            if (pBufferNew == NULL)
            {
                break;
            }
            AUTO_RESOURCE(pBufferNew, [](auto p) { ExFreePoolWithTag(p, LOGGER_POOL_TAG); });

            memcpy(pBufferNew, LgOutputBuffer, szBufferOldOffset);

            if (LgOutputBuffer != LgOutputBufferStatic)
            {
                ExFreePoolWithTag(LgOutputBuffer, LOGGER_POOL_TAG);
            }

            LgOutputBuffer = pBufferNew;
            LgOutputBufferCurrent = LgOutputBuffer + (szBufferOldOffset / sizeof(WCHAR));
            LgOutputBufferSize = szBufferNewSize;

            pBufferNew = NULL;
        }
        else
        {
            LgOutputBufferCurrent += szCharToWrite;
            break;
        }
    }
}

bool
Logger::_Log(LogLevel level, const char* file, int line, const char* function)
{
    if (level < LOGGER_MINIMUM_LEVEL)
    {
        return false;
    }

    // TODO: File name check.

    _Lock();

    LgCurrentLogLevel = level;

    // Extract name from file path.
    size_t pathLength = strlen(file);
    int lastComponent = (int)pathLength;
    while (lastComponent >= 0 && file[lastComponent] != '\\')
    {
        --lastComponent;
    }
    LgCurrentFile = file + lastComponent + 1;

    LgCurrentLine = line;
    LgCurrentFunction = function;

    return true;
}

void
Logger::_Lock()
{
    if (InterlockedCompareExchange(&LgInitialized, 1, 0))
    {
        LARGE_INTEGER lInt
        {
            .QuadPart = -100000
        };
        while (LgInitialized == 1)
        {
            KeDelayExecutionThread(KernelMode, FALSE, &lInt);
        }
    }
    else
    {
        // Won the race.
        ExInitializeFastMutex(&LgMutex);
        TraceLoggingRegister(LgTraceLoggingProvider);

        LgInitialized = 2;
    }

    ExAcquireFastMutex(&LgMutex);
}

void
Logger::_Unlock()
{
    ExReleaseFastMutex(&LgMutex);
}

void
Logger::_Flush()
{
    UNICODE_STRING strOutput
    {
        .Length = (USHORT)LgOutputBufferOffset,
        .MaximumLength = (USHORT)LgOutputBufferSize,
        .Buffer = LgOutputBuffer,
    };

    // DbgPrint

    CHAR cColumnFileLine[LOGGER_COLUMN_WIDTH + 1];
    int len = _snprintf(cColumnFileLine, LOGGER_COLUMN_WIDTH, "%hs:%i",
        LgCurrentFile, LgCurrentLine
    );
    for (int i = len; i < LOGGER_COLUMN_WIDTH; ++i)
    {
        cColumnFileLine[i] = ' ';
    }
    cColumnFileLine[LOGGER_COLUMN_WIDTH] = '\0';

    CHAR cColumnFunction[LOGGER_COLUMN_WIDTH + 1];
    len = _snprintf(cColumnFunction, LOGGER_COLUMN_WIDTH, "%hs", LgCurrentFunction);
    for (int i = len; i < LOGGER_COLUMN_WIDTH; ++i)
    {
        cColumnFunction[i] = ' ';
    }
    cColumnFunction[LOGGER_COLUMN_WIDTH] = '\0';

    PCSTR pLevelStr = "???";
    ULONG ulDbgPrintLevel = DPFLTR_MASK;

    switch (LgCurrentLogLevel)
    {
        case LogLevel::Trace:
            pLevelStr = "TRC";
            ulDbgPrintLevel = DPFLTR_INFO_LEVEL;
            break;
        case LogLevel::Info:
            pLevelStr = "INF";
            ulDbgPrintLevel = DPFLTR_TRACE_LEVEL;
            break;
        case LogLevel::Warning:
            pLevelStr = "WRN";
            ulDbgPrintLevel = DPFLTR_WARNING_LEVEL;
            break;
        case LogLevel::Error:
            pLevelStr = "ERR";
            ulDbgPrintLevel = DPFLTR_ERROR_LEVEL;
            break;
    }

#if DBG
    // Prevent debug strings from being filtered in WinDbg
    ulDbgPrintLevel = DPFLTR_ERROR_LEVEL;
#endif

    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        ulDbgPrintLevel,
        "%hs %hs%hs%wZ\n",
        pLevelStr, cColumnFileLine, cColumnFunction, &strOutput
    );

    // TraceLogging

#define LOGGER_WRITE(level)                                 \
    TraceLoggingWrite(                                      \
        LgTraceLoggingProvider,                             \
        "LxMonikaOutput",                                   \
        TraceLoggingLevel(level),                           \
        TraceLoggingString(pLevelStr, "Level"),             \
        TraceLoggingString(LgCurrentFile, "File"),          \
        TraceLoggingInt32(LgCurrentLine, "Line"),           \
        TraceLoggingString(LgCurrentFunction, "Function"),  \
        TraceLoggingUnicodeString(&strOutput, "Message")    \
    );

    switch (LgCurrentLogLevel)
    {
        case LogLevel::Trace:
            LOGGER_WRITE(WINEVENT_LEVEL_VERBOSE);
        break;
        case LogLevel::Info:
            LOGGER_WRITE(WINEVENT_LEVEL_INFO);
        break;
        case LogLevel::Warning:
            LOGGER_WRITE(WINEVENT_LEVEL_WARNING);
        break;
        case LogLevel::Error:
            LOGGER_WRITE(WINEVENT_LEVEL_ERROR);
        break;
        default:
            LOGGER_WRITE(WINEVENT_LEVEL_LOG_ALWAYS);
        break;
    }

#undef LOGGER_WRITE

    if (LgOutputBuffer != LgOutputBufferStatic)
    {
        ExFreePoolWithTag(LgOutputBuffer, LOGGER_POOL_TAG);

        LgOutputBuffer = LgOutputBufferStatic;
        LgOutputBufferSize = sizeof(LgOutputBufferStatic);
    }

    LgOutputBufferCurrent = LgOutputBuffer;
}

void
Logger::_Print(bool b)
{
    Write(L"%hs", b ? "true" : "false");
}

void
Logger::_Print(WORD w)
{
    Write(L"%hu", w);
}

void
Logger::_Print(DWORD dw)
{
    Write(L"%u", dw);
}

void
Logger::_Print(ULONGLONG ull)
{
    Write(L"%llu", ull);
}

void
Logger::_Print(INT i)
{
    Write(L"%i", i);
}

void
Logger::_Print(UINT i)
{
    Write(L"%u", i);
}

void
Logger::_Print(PSTR pStr)
{
    Write(L"%hs", pStr);
}

void
Logger::_Print(PCSTR pcStr)
{
    Write(L"%hs", pcStr);
}

void
Logger::_Print(PUCHAR pcuStr)
{
    Write(L"%hs", (const char*)pcuStr);
}

void
Logger::_Print(PWSTR pwStr)
{
    Write(L"%ws", pwStr);
}

void
Logger::_Print(PUNICODE_STRING puStr)
{
    Write(L"%wZ", puStr);
}

void
Logger::_Print(PVOID p)
{
    Write(L"%p", p);
}
