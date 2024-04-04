#pragma once

// condrv.h
//
// Support definitions for condrv interfaces.
// Based on https://github.com/microsoft/terminal/blob/main/dep/Console/condrv.h
// and https://github.com/microsoft/terminal/blob/main/dep/Console/conmsgl1.h
// Released by Microsoft Corporation under the MIT license.
//

#include <ntifs.h>

//
// Support definitions not available to kernel mode drivers.
//

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// wincontypes.h
typedef struct _COORD {
    SHORT X;
    SHORT Y;
} COORD, *PCOORD;

//
// ConDrv support functions provided by lxmonika.
//

#ifdef __cplusplus
extern "C"
{
#endif

    NTSTATUS
        CdpKernelConsoleAttach(
            _In_ HANDLE hdlOwnerProcess,
            _Out_ PHANDLE pHdlConsole
        );

    NTSTATUS
        CdpKernelConsoleOpenHandles(
            _In_ HANDLE hdlConsole,
            _Out_opt_ PHANDLE pHdlInput,
            _Out_opt_ PHANDLE pHdlOutput
        );

#ifdef __cplusplus
}
#endif

#pragma region condrv.h

/*++

Copyright (c) Microsoft Corporation. All rights reserved.
Licensed under the MIT license.

Module Name:

    condrv.h

Abstract:

    This module contains the declarations shared by the console driver and the
    user-mode components that use it.

Author:

    Wedson Almeida Filho (wedsonaf) 24-Sep-2009

Environment:

    Kernel and user modes.

--*/

#pragma once

//#include "..\NT\ntioapi_x.h"

//
// Messages that can be received by servers, used in CD_IO_DESCRIPTOR::Function.
//

#define CONSOLE_IO_CONNECT        0x01
#define CONSOLE_IO_DISCONNECT     0x02
#define CONSOLE_IO_CREATE_OBJECT  0x03
#define CONSOLE_IO_CLOSE_OBJECT   0x04
#define CONSOLE_IO_RAW_WRITE      0x05
#define CONSOLE_IO_RAW_READ       0x06
#define CONSOLE_IO_USER_DEFINED   0x07
#define CONSOLE_IO_RAW_FLUSH      0x08

//
// Header of all IOs submitted to a server.
//

typedef struct _CD_IO_DESCRIPTOR {
    LUID Identifier;
    ULONG_PTR Process;
    ULONG_PTR Object;
    ULONG Function;
    ULONG InputSize;
    ULONG OutputSize;
    ULONG Reserved;
} CD_IO_DESCRIPTOR, *PCD_IO_DESCRIPTOR;

//
// Types of objects, used in CREATE_OBJECT_INFORMATION::ObjectType.
//

#define CD_IO_OBJECT_TYPE_CURRENT_INPUT   0x01
#define CD_IO_OBJECT_TYPE_CURRENT_OUTPUT  0x02
#define CD_IO_OBJECT_TYPE_NEW_OUTPUT      0x03
#define CD_IO_OBJECT_TYPE_GENERIC         0x04

//
// Payload of the CONSOLE_IO_CREATE_OBJECT io.
//

typedef struct _CD_CREATE_OBJECT_INFORMATION {
    ULONG ObjectType;
    ULONG ShareMode;
    ACCESS_MASK DesiredAccess;
} CD_CREATE_OBJECT_INFORMATION, *PCD_CREATE_OBJECT_INFORMATION;

//
// Create EA buffers.
//

#define CD_BROKER_EA_NAME "broker"
#define CD_SERVER_EA_NAME "server"
#define CD_ATTACH_EA_NAME "attach"

typedef struct _CD_CREATE_SERVER {
    HANDLE BrokerHandle;
    LUID BrokerRequest;
} CD_CREATE_SERVER, *PCD_CREATE_SERVER;

typedef struct _CD_ATTACH_INFORMATION {
    HANDLE ProcessId;
} CD_ATTACH_INFORMATION, *PCD_ATTACH_INFORMATION;

typedef struct _CD_ATTACH_INFORMATION64 {
    PVOID64 ProcessId;
} CD_ATTACH_INFORMATION64, *PCD_ATTACH_INFORMATION64;

//
// Information passed to the driver by a server when a connection is accepted.
//

typedef struct _CD_CONNECTION_INFORMATION {
    ULONG_PTR Process;
    ULONG_PTR Input;
    ULONG_PTR Output;
} CD_CONNECTION_INFORMATION, *PCD_CONNECTION_INFORMATION;

//
// Ioctls.
//

typedef struct _CD_IO_BUFFER {
    ULONG Size;
    PVOID Buffer;
} CD_IO_BUFFER, *PCD_IO_BUFFER;

typedef struct _CD_IO_BUFFER64 {
    ULONG Size;
    PVOID64 Buffer;
} CD_IO_BUFFER64, *PCD_IO_BUFFER64;

typedef struct _CD_USER_DEFINED_IO {
    HANDLE Client;
    ULONG InputCount;
    ULONG OutputCount;
    CD_IO_BUFFER Buffers[ANYSIZE_ARRAY];
} CD_USER_DEFINED_IO, *PCD_USER_DEFINED_IO;

typedef struct _CD_USER_DEFINED_IO64 {
    PVOID64 Client;
    ULONG InputCount;
    ULONG OutputCount;
    CD_IO_BUFFER64 Buffers[ANYSIZE_ARRAY];
} CD_USER_DEFINED_IO64, *PCD_USER_DEFINED_IO64;

typedef struct _CD_IO_BUFFER_DESCRIPTOR {
    PVOID Data;
    ULONG Size;
    ULONG Offset;
} CD_IO_BUFFER_DESCRIPTOR, *PCD_IO_BUFFER_DESCRIPTOR;

typedef struct _CD_IO_COMPLETE {
    LUID Identifier;
    IO_STATUS_BLOCK IoStatus;
    CD_IO_BUFFER_DESCRIPTOR Write;
} CD_IO_COMPLETE, *PCD_IO_COMPLETE;

typedef struct _CD_IO_OPERATION {
    LUID Identifier;
    CD_IO_BUFFER_DESCRIPTOR Buffer;
} CD_IO_OPERATION, *PCD_IO_OPERATION;

typedef struct _CD_IO_SERVER_INFORMATION {
    HANDLE InputAvailableEvent;
} CD_IO_SERVER_INFORMATION, *PCD_IO_SERVER_INFORMATION;

typedef struct _CD_IO_DISPLAY_SIZE {
    ULONG Width;
    ULONG Height;
} CD_IO_DISPLAY_SIZE, *PCD_IO_DISPLAY_SIZE;

typedef struct _CD_IO_CHARACTER {
    WCHAR Character;
    USHORT Attribute;
} CD_IO_CHARACTER, *PCD_IO_CHARACTER;

typedef struct _CD_IO_ROW_INFORMATION {
    SHORT Index;
    PCD_IO_CHARACTER Old;
    PCD_IO_CHARACTER New;
} CD_IO_ROW_INFORMATION, *PCD_IO_ROW_INFORMATION;

typedef struct _CD_IO_CURSOR_INFORMATION {
    USHORT Column;
    USHORT Row;
    ULONG Height;
    BOOLEAN IsVisible;
} CD_IO_CURSOR_INFORMATION, *PCD_IO_CURSOR_INFORMATION;

typedef struct _CD_IO_FONT_SIZE {
    ULONG Width;
    ULONG Height;
} CD_IO_FONT_SIZE, *PCD_IO_FONT_SIZE;

#define IOCTL_CONDRV_READ_IO \
    CTL_CODE(FILE_DEVICE_CONSOLE, 1, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_COMPLETE_IO \
    CTL_CODE(FILE_DEVICE_CONSOLE, 2, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_READ_INPUT \
    CTL_CODE(FILE_DEVICE_CONSOLE, 3, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_WRITE_OUTPUT \
    CTL_CODE(FILE_DEVICE_CONSOLE, 4, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_ISSUE_USER_IO \
    CTL_CODE(FILE_DEVICE_CONSOLE, 5, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_DISCONNECT_PIPE \
    CTL_CODE(FILE_DEVICE_CONSOLE, 6, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_SET_SERVER_INFORMATION \
    CTL_CODE(FILE_DEVICE_CONSOLE, 7, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_GET_SERVER_PID \
    CTL_CODE(FILE_DEVICE_CONSOLE, 8, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_GET_DISPLAY_SIZE \
    CTL_CODE(FILE_DEVICE_CONSOLE, 9, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_UPDATE_DISPLAY \
    CTL_CODE(FILE_DEVICE_CONSOLE, 10, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_SET_CURSOR \
    CTL_CODE(FILE_DEVICE_CONSOLE, 11, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_ALLOW_VIA_UIACCESS \
    CTL_CODE(FILE_DEVICE_CONSOLE, 12, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_LAUNCH_SERVER \
    CTL_CODE(FILE_DEVICE_CONSOLE, 13, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_GET_FONT_SIZE \
    CTL_CODE(FILE_DEVICE_CONSOLE, 14, METHOD_NEITHER, FILE_ANY_ACCESS)

#pragma endregion condrv.h

#pragma region conmsgl1.h

/*++

Copyright (c) Microsoft Corporation. All rights reserved.
Licensed under the MIT license.

Module Name:

    conmsgl1.h

Abstract:

    This include file defines the layer 1 message formats used to communicate
    between the client and server portions of the CONSOLE portion of the
    Windows subsystem.

Author:

    Therese Stowell (thereses) 10-Nov-1990

Revision History:

    Wedson Almeida Filho (wedsonaf) 23-May-2010
        Modified the messages for use with the console driver.

--*/

#pragma once

#define CONSOLE_FIRST_API_NUMBER(Layer) \
    (Layer << 24) \

typedef struct _CONSOLE_SERVER_MSG {
    ULONG IconId;
    ULONG HotKey;
    ULONG StartupFlags;
    USHORT FillAttribute;
    USHORT ShowWindow;
    COORD ScreenBufferSize;
    COORD WindowSize;
    COORD WindowOrigin;
    ULONG ProcessGroupId;
    BOOLEAN ConsoleApp;
    BOOLEAN WindowVisible;
    USHORT TitleLength;
    WCHAR Title[MAX_PATH + 1];
    USHORT ApplicationNameLength;
    WCHAR ApplicationName[128];
    USHORT CurrentDirectoryLength;
    WCHAR CurrentDirectory[MAX_PATH + 1];
} CONSOLE_SERVER_MSG, *PCONSOLE_SERVER_MSG;

typedef struct _CONSOLE_BROKER_DATA {
    WCHAR DesktopName[MAX_PATH];
} CONSOLE_BROKER_MSG, *PCONSOLE_BROKER_MSG;

typedef struct _CONSOLE_GETCP_MSG {
    OUT ULONG CodePage;
    IN BOOLEAN Output;
} CONSOLE_GETCP_MSG, *PCONSOLE_GETCP_MSG;

typedef struct _CONSOLE_MODE_MSG {
    IN OUT ULONG Mode;
} CONSOLE_MODE_MSG, *PCONSOLE_MODE_MSG;

typedef struct _CONSOLE_GETNUMBEROFINPUTEVENTS_MSG {
    OUT ULONG ReadyEvents;
} CONSOLE_GETNUMBEROFINPUTEVENTS_MSG, *PCONSOLE_GETNUMBEROFINPUTEVENTS_MSG;

typedef struct _CONSOLE_GETCONSOLEINPUT_MSG {
    OUT ULONG NumRecords;
    IN USHORT Flags;
    IN BOOLEAN Unicode;
} CONSOLE_GETCONSOLEINPUT_MSG, *PCONSOLE_GETCONSOLEINPUT_MSG;

typedef struct _CONSOLE_READCONSOLE_MSG {
    IN BOOLEAN Unicode;
    IN BOOLEAN ProcessControlZ;
    IN USHORT ExeNameLength;
    IN ULONG InitialNumBytes;
    IN ULONG CtrlWakeupMask;
    OUT ULONG ControlKeyState;
    OUT ULONG NumBytes;
} CONSOLE_READCONSOLE_MSG, *PCONSOLE_READCONSOLE_MSG;

typedef struct _CONSOLE_WRITECONSOLE_MSG {
    OUT ULONG NumBytes;
    IN BOOLEAN Unicode;
} CONSOLE_WRITECONSOLE_MSG, *PCONSOLE_WRITECONSOLE_MSG;

typedef struct _CONSOLE_LANGID_MSG {
    OUT LANGID LangId;
} CONSOLE_LANGID_MSG, *PCONSOLE_LANGID_MSG;

typedef struct _CONSOLE_MAPBITMAP_MSG {
    OUT HANDLE Mutex;
    OUT PVOID Bitmap;
} CONSOLE_MAPBITMAP_MSG, *PCONSOLE_MAPBITMAP_MSG;

typedef struct _CONSOLE_MAPBITMAP_MSG64 {
    OUT PVOID64 Mutex;
    OUT PVOID64 Bitmap;
} CONSOLE_MAPBITMAP_MSG64, *PCONSOLE_MAPBITMAP_MSG64;

typedef enum _CONSOLE_API_NUMBER_L1 {
    ConsolepGetCP = CONSOLE_FIRST_API_NUMBER(1),
    ConsolepGetMode,
    ConsolepSetMode,
    ConsolepGetNumberOfInputEvents,
    ConsolepGetConsoleInput,
    ConsolepReadConsole,
    ConsolepWriteConsole,
    ConsolepNotifyLastClose,
    ConsolepGetLangId,
    ConsolepMapBitmap,
} CONSOLE_API_NUMBER_L1, *PCONSOLE_API_NUMBER_L1;

typedef struct _CONSOLE_MSG_HEADER {
    ULONG ApiNumber;
    ULONG ApiDescriptorSize;
} CONSOLE_MSG_HEADER, *PCONSOLE_MSG_HEADER;

typedef union _CONSOLE_MSG_BODY_L1 {
    CONSOLE_GETCP_MSG GetConsoleCP;
    CONSOLE_MODE_MSG GetConsoleMode;
    CONSOLE_MODE_MSG SetConsoleMode;
    CONSOLE_GETNUMBEROFINPUTEVENTS_MSG GetNumberOfConsoleInputEvents;
    CONSOLE_GETCONSOLEINPUT_MSG GetConsoleInput;
    CONSOLE_READCONSOLE_MSG ReadConsole;
    CONSOLE_WRITECONSOLE_MSG WriteConsole;
    CONSOLE_LANGID_MSG GetConsoleLangId;

#if defined(BUILD_WOW6432) && !defined(BUILD_WOW3232)

    CONSOLE_MAPBITMAP_MSG64 MapBitmap;

#else

    CONSOLE_MAPBITMAP_MSG MapBitmap;

#endif

} CONSOLE_MSG_BODY_L1, *PCONSOLE_MSG_BODY_L1;

#ifndef __cplusplus
typedef struct _CONSOLE_MSG_L1 {
    CONSOLE_MSG_HEADER Header;
    union {
        CONSOLE_MSG_BODY_L1;
    } u;
} CONSOLE_MSG_L1, *PCONSOLE_MSG_L1;
#else
typedef struct _CONSOLE_MSG_L1 :
    public CONSOLE_MSG_HEADER
{
    CONSOLE_MSG_BODY_L1 u;
} CONSOLE_MSG_L1, *PCONSOLE_MSG_L1;
#endif // __cplusplus

#pragma endregion conmsgl1.h
