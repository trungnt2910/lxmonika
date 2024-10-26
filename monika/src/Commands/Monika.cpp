#include "Commands/Monika.h"

#include <iostream>

#include <Windows.h>
#include <winternl.h>

#include <lxmonika/reality.h>

#include "resource.h"
#include "service.h"
#include "util.h"

#include "Exception.h"

Monika::Monika()
  : Command(
        MA_STRING_MONIKA_COMMAND_NAME,
        MA_STRING_MONIKA_COMMAND_DESCRIPTION,
        NullSwitch
    ),
    _execCommand(this),
    _installCommand(this),
    _uninstallCommand(this),
    _infoSwitch(
        MA_STRING_MONIKA_SWITCH_INFO_NAME,
        -1,
        MA_STRING_MONIKA_SWITCH_INFO_DESCRIPTION,
        NullParameter,
        _shouldPrintInfo,
        false
    )
{
    AddCommand(_execCommand);
    AddCommand(_installCommand);
    AddCommand(_uninstallCommand);

    AddSwitch(_infoSwitch);
}

int
Monika::Execute() const
{
    if (_shouldPrintInfo)
    {
        auto manager = UtilGetSharedServiceHandle(OpenSCManagerW(
            NULL, NULL, GENERIC_READ
        ));

        if (!SvIsLxMonikaInstalled(manager))
        {
            throw MonikaException(MA_STRING_EXCEPTION_LXMONIKA_NOT_INSTALLED);
        }

        auto file = UtilGetSharedWin32Handle(CreateFileW(
            L"\\\\?\\GLOBALROOT" RL_DEVICE_NAME,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        ));

        // Reality device data is in UTF-8.
        std::string buffer;
        buffer.resize(4096);

        // The file is small, so cache everything in memory first.
        // Writing UTF-8 directy with std::cout.write can cause issues.
        std::stringstream streamContents;

        OVERLAPPED offset;
        memset(&offset, 0, sizeof(offset));
        offset.Offset = 0;

        while (true)
        {
            DWORD cbBytesRead = 0;

            std::ignore = ReadFile(
                file.get(),
                buffer.data(),
                (DWORD)buffer.size(),
                &cbBytesRead,
                &offset
            );
            Win32Exception::ThrowUnless(ERROR_HANDLE_EOF);

            if (cbBytesRead == 0)
            {
                break;
            }

            streamContents.write(buffer.c_str(), cbBytesRead);

            // Increment the offsets, since they are not automatically updated.
            LARGE_INTEGER largeOffsets;
            largeOffsets.HighPart = offset.OffsetHigh;
            largeOffsets.LowPart = offset.Offset;

            largeOffsets.QuadPart += cbBytesRead;

            offset.Offset = largeOffsets.LowPart;
            offset.OffsetHigh = largeOffsets.HighPart;
        }

        buffer = streamContents.str();

        int cbMultiByte = Win32Exception::ThrowIfNull(MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            buffer.c_str(),
            (int)buffer.size(),
            nullptr,
            0
        ));

        std::wstring wideBuffer;
        wideBuffer.resize(cbMultiByte);

        std::ignore = Win32Exception::ThrowIfNull(MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            buffer.c_str(),
            (int)buffer.size(),
            wideBuffer.data(),
            (int)wideBuffer.size()
        ));

        std::wcout << wideBuffer << std::flush;

        return 0;
    }
    else
    {
        std::wcout << UtilGetResourceString(MA_STRING_MONIKA_JUST_MONIKA) << std::endl;
        return 0;
    }
}
