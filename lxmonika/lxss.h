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
