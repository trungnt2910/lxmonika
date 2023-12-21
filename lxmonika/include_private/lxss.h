#pragma once

// lxss.h
//
// Support definitions for lxcore internal functions.
// Based on https://github.com/billziss-gh/lxdk/blob/master/inc/lxdk/lxdk.h
// Released by Bill Zissimopoulos under the LGPLv3
//
/**
 * @file lxdk/lxdk.h
 *
 * @copyright 2019-2020 Bill Zissimopoulos
 */
 /*
  * This file is part of LxDK.
  *
  * You can redistribute it and/or modify it under the terms of the GNU
  * Lesser General Public License version 3 as published by the Free
  * Software Foundation.
  */
//
// Since lxmonika only consumes this header's definitions and does not distribute lxss.h as part
// of the built product (only monika.h is released to the public), this project should not be
// infected by the LGPLv3.

#include <ntddk.h>

#pragma warning(disable: 4200)           /* zero-sized array in struct/union */
#pragma warning(disable: 4201)           /* nameless struct/union */

typedef INT64 OFF_T, *POFF_T;

typedef struct _LX_SUBSYSTEM LX_SUBSYSTEM, *PLX_SUBSYSTEM;
typedef struct _LX_INSTANCE LX_INSTANCE, *PLX_INSTANCE;
typedef struct _LX_VFS_STARTUP_ENTRY LX_VFS_STARTUP_ENTRY, *PLX_VFS_STARTUP_ENTRY;
typedef struct _LX_DEVICE LX_DEVICE, *PLX_DEVICE;
typedef struct _LX_DEVICE_CALLBACKS LX_DEVICE_CALLBACKS, *PLX_DEVICE_CALLBACKS;
typedef struct _LX_INODE LX_INODE, *PLX_INODE;
typedef struct _LX_INODE_CALLBACKS LX_INODE_CALLBACKS, *PLX_INODE_CALLBACKS;
typedef struct _LX_INODE_XATTR_CALLBACKS LX_INODE_XATTR_CALLBACKS, *PLX_INODE_XATTR_CALLBACKS;
typedef struct _LX_FILE LX_FILE, *PLX_FILE;
typedef struct _LX_FILE_CALLBACKS LX_FILE_CALLBACKS, *PLX_FILE_CALLBACKS;
typedef struct _LX_CALL_CONTEXT LX_CALL_CONTEXT, *PLX_CALL_CONTEXT;
typedef struct _LX_IOVECTOR LX_IOVECTOR, *PLX_IOVECTOR;
typedef struct _LX_IOVECTOR_BUFFER LX_IOVECTOR_BUFFER, *PLX_IOVECTOR_BUFFER;

typedef INT LX_SUBSYSTEM_CREATE_INITIAL_NAMESPACE(
    _In_ PLX_INSTANCE Instance);

typedef LX_SUBSYSTEM_CREATE_INITIAL_NAMESPACE* PLX_SUBSYSTEM_CREATE_INITIAL_NAMESPACE;

typedef INT LX_DEVICE_OPEN(
    _In_ PLX_CALL_CONTEXT CallContext,
    _In_ PLX_DEVICE Device,
    _In_ ULONG Flags,
    _Out_ PLX_FILE* PFile);
typedef INT LX_DEVICE_DELETE(
    _Inout_ PLX_DEVICE Device);

typedef LX_DEVICE_OPEN* PLX_DEVICE_OPEN;
typedef LX_DEVICE_DELETE* PLX_DEVICE_DELETE;

typedef INT LX_FILE_DELETE(
    _In_ PLX_CALL_CONTEXT CallContext,
    _Inout_ PLX_FILE File);
typedef INT LX_FILE_FLUSH(
    _In_ PLX_CALL_CONTEXT CallContext,
    _Inout_ PLX_FILE File);
typedef INT LX_FILE_IOCTL(
    _In_ PLX_CALL_CONTEXT CallContext,
    _Inout_ PLX_FILE File,
    _In_ ULONG Code,
    _Inout_ PVOID Buffer);
typedef INT LX_FILE_READ(
    _In_ PLX_CALL_CONTEXT CallContext,
    _Inout_ PLX_FILE File,
    _Out_ PVOID Buffer,
    _In_ SIZE_T Length,
    _Inout_opt_ POFF_T POffset,
    _Out_ PSIZE_T PBytesTransferred);
typedef INT LX_FILE_READ_VECTOR(
    _In_ PLX_CALL_CONTEXT CallContext,
    _Inout_ PLX_FILE File,
    _In_ PLX_IOVECTOR IoVector,
    _Inout_ POFF_T POffset,
    _In_ ULONG Flags,
    _Out_ PSIZE_T PBytesTransferred);
typedef INT LX_FILE_RELEASE(
    _In_ PLX_CALL_CONTEXT CallContext,
    _Inout_ PLX_FILE File);
typedef INT LX_FILE_SEEK(
    _In_ PLX_CALL_CONTEXT CallContext,
    _Inout_ PLX_FILE File,
    _In_ OFF_T Offset,
    _In_ INT Whence,
    _Out_ POFF_T PResultOffset);
typedef INT LX_FILE_WRITE(
    _In_ PLX_CALL_CONTEXT CallContext,
    _Inout_ PLX_FILE File,
    _In_ PVOID Buffer,
    _In_ SIZE_T Length,
    _Inout_ POFF_T POffset,
    _Out_ PSIZE_T PBytesTransferred);
typedef INT LX_FILE_WRITE_VECTOR(
    _In_ PLX_CALL_CONTEXT CallContext,
    _Inout_ PLX_FILE File,
    _In_ PLX_IOVECTOR IoVector,
    _Inout_ POFF_T POffset,
    _In_ ULONG Flags,
    _Out_ PSIZE_T PBytesTransferred);

typedef LX_FILE_DELETE* PLX_FILE_DELETE;
typedef LX_FILE_FLUSH* PLX_FILE_FLUSH;
typedef LX_FILE_IOCTL* PLX_FILE_IOCTL;
typedef LX_FILE_READ* PLX_FILE_READ;
typedef LX_FILE_READ_VECTOR* PLX_FILE_READ_VECTOR;
typedef LX_FILE_RELEASE* PLX_FILE_RELEASE;
typedef LX_FILE_SEEK* PLX_FILE_SEEK;
typedef LX_FILE_WRITE* PLX_FILE_WRITE;
typedef LX_FILE_WRITE_VECTOR* PLX_FILE_WRITE_VECTOR;

struct _LX_SUBSYSTEM
{
    PLX_SUBSYSTEM_CREATE_INITIAL_NAMESPACE CreateInitialNamespace;

    PVOID Reserved[7];
};

enum
{
    VfsStartupEntryDirectory,
    VfsStartupEntryMount,
    VfsStartupEntryNode,
    VfsStartupEntrySymlink,
    VfsStartupEntryFile,
};

struct _LX_VFS_STARTUP_ENTRY
{
    ULONG Kind;
    UNICODE_STRING Path;
    union
    {
        struct
        {
            ULONG Uid;
            ULONG Gid;
            ULONG Mode;
        } Directory;
        UINT8 Mount[72];
        struct
        {
            ULONG Uid;
            ULONG Gid;
            ULONG Mode;
            UINT32 DeviceMinor : 20;
            UINT32 DeviceMajor : 12;
        } Node;
        struct
        {
            UNICODE_STRING TargetPath;
        } Symlink;
        struct
        {
            ULONG Mode;
        } File;
    } DUMMYUNIONNAME;
};

struct _LX_DEVICE
{
    UINT64 Reserved[32];
};

struct _LX_FILE
{
    UINT64 Reserved[1];
};

struct _LX_DEVICE_CALLBACKS
{
    PLX_DEVICE_OPEN Open;
    PLX_DEVICE_DELETE Delete;

    PVOID Reserved[6];
};

struct _LX_INODE_XATTR_CALLBACKS
{
    PVOID GetExtendedAttribute;
    PVOID SetExtendedAttribute;
    PVOID RemoveExtendedAttribute;

    PVOID Reserved[5];
};

struct _LX_INODE_CALLBACKS
{
    PVOID Open;
    PVOID Delete;
    PVOID Lookup;
    PVOID Stat;
    PVOID Chown;
    PVOID CreateFile;
    PVOID Symlink;
    PVOID Link;
    PVOID CreateDirectory;
    PVOID Unlink;
    PVOID Rmdir;
    PVOID Rename;
    PVOID ReadLink;
    PVOID CreateNode;
    PVOID Chmod;
    PVOID PrePermissionsCheck;
    PVOID PostPermissionsCheck;
    PVOID SetTimes;
    PVOID ListExtendedAttributes;
    PVOID FollowLink;
    PVOID InotifyNtWatchDecrementWatchCount;
    PVOID InotifyNtWatchIncrementWatchCount;
    PVOID ReferenceNtFileObject;
    PVOID Pin;
    struct LX_INODE_XATTR_CALLBACKS* ExtendedAttributeCallbacks;
    struct LX_INODE_XATTR_CALLBACKS* SystemExtendedAttributeCallbacks;

    PVOID Reserved[6];
};

struct _LX_FILE_CALLBACKS
{
    PLX_FILE_DELETE Delete;
    PLX_FILE_READ Read;
    PVOID ReadDir;
    PLX_FILE_WRITE Write;
    PLX_FILE_WRITE_VECTOR WriteVector;
    PVOID Map;
    PVOID MapManual;
    PLX_FILE_IOCTL Ioctl;
    PLX_FILE_FLUSH Flush;
    PVOID Sync;
    PLX_FILE_RELEASE Release;
    PLX_FILE_READ_VECTOR ReadVector;
    PVOID Truncate;
    PLX_FILE_SEEK Seek;
    PVOID FilterPollRegistration;
    PVOID FAllocate;
    PVOID GetPathString;
    PVOID GetNtDeviceType;

    PVOID Reserved[14];
};

struct _LX_IOVECTOR_BUFFER
{
    PVOID Buffer;
    SIZE_T Length;
};

struct _LX_IOVECTOR
{
    INT Count;
    LX_IOVECTOR_BUFFER Vector[];
};

//
// Additional structs
//
// These structs are NOT part of LxDK. Licensed under the the MIT License.
// There have been no real-world tests for the validity of these structures,
// USE THEM AT YOUR OWN RISK.

typedef struct _LX_VFS_DEVICE LX_VFS_DEVICE, *PLX_VFS_DEVICE;

typedef INT LX_VFS_DEVICE_OPEN(
    _In_ PLX_CALL_CONTEXT CallContext,
    _In_ PLX_VFS_DEVICE Device,
    _In_ ULONG Flags,
    _Out_ PVOID* PFile);
typedef INT LX_VFS_DEVICE_DELETE(
    _Inout_ PLX_VFS_DEVICE Device);

typedef LX_VFS_DEVICE_OPEN* PLX_VFS_DEVICE_OPEN;
typedef LX_VFS_DEVICE_DELETE* PLX_VFS_DEVICE_DELETE;

typedef struct _LX_VFS_DEVICE_TYPE {
    PLX_VFS_DEVICE_OPEN Open;
    PLX_VFS_DEVICE_DELETE Delete;
} LX_VFS_DEVICE_TYPE, *PLX_VFS_DEVICE_TYPE;

typedef struct _LX_VFS_DEVICE {
    ULONG_PTR ReferenceCount;
    PLX_VFS_DEVICE_TYPE Type;
    UINT64 Reserved[5];
} LX_VFS_DEVICE, *PLX_VFS_DEVICE;

typedef struct _LX_VFS_FILE_TYPE {
    PVOID Delete;
    PVOID Reserved;
    PVOID Reserved1;
    PVOID Reserved2;
    PVOID WriteVector;
    PVOID Map;
    PVOID Reserved3;
    PVOID Ioctl;
    PVOID Reserved4;
    PVOID Reserved5;
    PVOID Release;
    PVOID ReadVector;
    PVOID Reserved6;
    PVOID Reserved7;
    PVOID Reserved8;
    PVOID Reserved9;
    PVOID GetPathString;
    PVOID GetNtDeviceType;
    PVOID Reserved10;
    PVOID Reserved11;
} LX_VFS_FILE_TYPE, *PLX_VFS_FILE_TYPE;

typedef struct _LX_VFS_FILE {
    UINT64 Reserved[3];
    PLX_VFS_FILE_TYPE Type;
    ULONG_PTR LifetimeReferenceCount;
    UINT64 Reserved1;
    ULONG_PTR ReferenceCount;
    UINT64 Reserved2[9];
} LX_VFS_FILE, *PLX_VFS_FILE;

typedef struct _LX_VFS_FILE_DESCRIPTOR {
    ULONG_PTR File;
    UINT64 Reserved;
} LX_VFS_FILE_DESCRIPTOR, *PLX_VFS_FILE_DESCRIPTOR;

typedef struct _LX_VFS_DEV_TTY_CONSOLE_STATE {
    HANDLE Input;
    HANDLE Output;
    HANDLE Console;
    UINT64 Reserved[4];
    PEPROCESS Process;
} LX_VFS_DEV_TTY_CONSOLE_STATE, *PLX_VFS_DEV_TTY_CONSOLE_STATE;

typedef struct _LX_VFS_DEV_TTY_DEVICE {
    ULONG_PTR ReferenceCount;
    PLX_VFS_DEVICE_TYPE Type;
    UINT64 Reserved[20];
    PLX_VFS_DEV_TTY_CONSOLE_STATE ConsoleState;
    UINT64 Reserved1[2];
} LX_VFS_DEV_TTY_DEVICE, *PLX_VFS_DEV_TTY_DEVICE;

typedef struct _LX_STRING {
    SIZE_T Length;
    SIZE_T MaximumLength;
    PCHAR Buffer;
} LX_STRING, *PLX_STRING;

typedef struct _LX_WAIT_OBJECT *PLX_WAIT_OBJECT;

typedef VOID LX_WAIT_OBJECT_LAST_DEREFERENCE(
    _In_ PLX_CALL_CONTEXT CallContext,
    _In_ PLX_WAIT_OBJECT WaitObject
);
typedef LX_WAIT_OBJECT_LAST_DEREFERENCE* PLX_WAIT_OBJECT_LAST_DEREFERENCE;

typedef struct _LX_WAIT_OBJECT_TYPE {
    PLX_WAIT_OBJECT_LAST_DEREFERENCE LastDereference;
} LX_WAIT_OBJECT_TYPE, *PLX_WAIT_OBJECT_TYPE;

typedef struct _LX_WAIT_OBJECT {
    PLX_WAIT_OBJECT_TYPE Type;
    EX_PUSH_LOCK Lock;
    UINT64 Reserved[1];
    NTSTATUS Status;
    UINT32 Reserved2;
    ULONG_PTR ReferenceCount;
    LIST_ENTRY Events;
    LIST_ENTRY ThreadGroups;
    UINT64 Reserved3[5];
} LX_WAIT_OBJECT, *PLX_WAIT_OBJECT;

typedef struct _LX_GLOBAL {
    PDEVICE_OBJECT ControlDevice;
    LIST_ENTRY Instances;
    EX_PUSH_LOCK Lock;
} LX_GLOBAL, *PLX_GLOBAL;

typedef struct _LX_INSTANCE {
    LIST_ENTRY ListEntry;
    ULONG_PTR ReferenceCount;
    EX_RUNDOWN_REF RundownProtection;
    PDEVICE_OBJECT ControlDevice;
    GUID Guid;
    UINT64 Reserved2[30];
    LIST_ENTRY Sessions;
    UINT64 Reserved3[5];
    EX_PUSH_LOCK Lock;
    UINT64 Reserved4[264];
} LX_INSTANCE, *PLX_INSTANCE;

typedef struct _LX_SESSION {
    LX_WAIT_OBJECT WaitObject;
    LIST_ENTRY ListEntry;
    PLX_VFS_DEVICE SessionTerminal;
    UINT64 Reserved;
    LIST_ENTRY ProcessGroups;
    UINT64 Reserved1;
} LX_SESSION, *PLX_SESSION;

typedef struct _LX_PROCESS_GROUP {
    LX_WAIT_OBJECT WaitObject;
    LIST_ENTRY ListEntry;
    LIST_ENTRY ThreadGroups;
    PLX_SESSION Session;
} LX_PROCESS_GROUP, *PLX_PROCESS_GROUP;

typedef struct _LX_PROCESS {
    PLX_INSTANCE Instance;
    PEPROCESS Process;
    ULONG_PTR ReferenceCount;
    HANDLE Handle;
    UINT64 Reserved1[2];
    EX_PUSH_LOCK Lock;
    LIST_ENTRY ThreadGroups;
    UINT64 Reserved2[1];
    UNICODE_STRING ExecutablePath;
    UINT64 Reserved3[39];
} LX_PROCESS, *PLX_PROCESS;

typedef struct _LX_THREAD_GROUP {
    LX_WAIT_OBJECT WaitObject;
    UINT64 Reserved1[3];
    PLX_PROCESS Process;
    PLX_INSTANCE Instance;
    UINT64 Reserved2[5];
    PLX_PROCESS_GROUP ProcessGroup;
    UINT64 Reserved3[1];
    LIST_ENTRY ListEntry;
    UINT64 Reserved4;
    EX_RUNDOWN_REF RundownProtection;
    UINT64 Reserved5[3];
    LIST_ENTRY Threads;
    UINT32 ThreadCount;
    UINT32 Reserved6;
    UINT64 Reserved7[720];
} LX_THREAD_GROUP, *PLX_THREAD_GROUP;

typedef struct _LX_THREAD_GROUP_LAUNCH_PARAMETERS {
    PLX_STRING Reserved;
    UINT64 Reserved1[31];
} LX_THREAD_GROUP_LAUNCH_PARAMETERS, *PLX_THREAD_GROUP_LAUNCH_PARAMETERS;

typedef struct _LX_FILE_DESCRIPTOR_TABLE {
    ULONG_PTR ReferenceCount;
    UINT64 Reserved;
    EX_PUSH_LOCK Lock;
    UINT32 Size;
    UINT32 Opened;
    UINT32 Capacity;
    UINT32 Reserved1;
    PLX_VFS_FILE_DESCRIPTOR FileDescriptors;
    UINT64 Reserved3[4];
} LX_FILE_DESCRIPTOR_TABLE, *PLX_FILE_DESCRIPTOR_TABLE;

typedef struct _LX_FILE_SYSTEM {
    UINT64 ReferenceCount;
    UINT64 Reserved[8];
} LX_FILE_SYSTEM, *PLX_FILE_SYSTEM;

typedef struct _LX_NT_STATE_THREAD_DATA {
    UINT64 ReferenceCount;
    PETHREAD Thread;
    HANDLE Handle;
    KAPC KernelApc;
    KAPC UserApc;
} LX_NT_STATE_THREAD_DATA, *PLX_NT_STATE_THREAD_DATA;

typedef struct _LX_NT_STATE_THREAD {
    EX_PUSH_LOCK Lock;
    ULONG64 StartTime;
    ULONG64 KernelTime;
    ULONG64 UserTime;
    PLX_NT_STATE_THREAD_DATA Data;
} LX_NT_STATE_THREAD, *PLX_NT_STATE_THREAD;

typedef enum _LX_THREAD_FLAGS {
    LxThreadPreparedForTermination = 0x1,
    LxThreadTerminating = 0x10
} LX_THREAD_FLAGS;

typedef struct _LX_THREAD {
    LX_WAIT_OBJECT WaitObject;
    LIST_ENTRY ListItem;
    PLX_THREAD_GROUP ThreadGroup;
    PLX_FILE_DESCRIPTOR_TABLE FileDescriptorTable;
    PLX_FILE_SYSTEM FileSystem;
    UINT32 Reserved;
    LX_THREAD_FLAGS Flags;
    EX_RUNDOWN_REF RundownProtection;
    UINT64 Reserved1[498];
} LX_THREAD, *PLX_THREAD;

typedef struct _LX_VFS_LOOKUP_CONTEXT LX_VFS_LOOKUP_CONTEXT, *PLX_VFS_LOOKUP_CONTEXT;

typedef enum _LX_CALL_CONTEXT_FLAGS {
    LxCallContextHasThread = 0x2,
    LxCallContextHasLookupContext = 0x4
} LX_CALL_CONTEXT_FLAGS;

typedef struct _LX_CALL_CONTEXT {
    PLX_INSTANCE Instance;
    UINT64 Reserved[8];
    PLX_THREAD Thread;
    PLX_VFS_LOOKUP_CONTEXT VfsLookupContext;
    UINT64 Reserved1[2];
    LX_CALL_CONTEXT_FLAGS Flags;
    UINT32 Reserved2;
} LX_CALL_CONTEXT, *PLX_CALL_CONTEXT;
