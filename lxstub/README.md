# lxstub

[![Discord Invite](https://dcbadge.vercel.app/api/server/bcV3gXGtsJ?style=flat)](https://discord.gg/bcV3gXGtsJ)

Stub for `lxcore.sys`.

## Building

Build this as a normal Visual Studio project. The output will be `LXCORE.sys`.

Normally, this driver is not built directly but as part of a [`lxmonika`](../lxmonika) build.

## Installing

This driver is not meant to be deployed to the target system. Instead, a copy of `lxcore.sys`
included with WSL1 (for 64-bit OSes) or AoW/Project Astoria (for 32-bit OSes) should be installed.

On systems with neither WSL nor AoW, in case you cannot or do not wish to deploy the genuine
`lxcore.sys`, the provided stub can be passed to the [`monika.exe`](../monika) CLI while installing
[`lxmonika`](../lxmonika/README.md#build-instructions).
```cmd
monika install path\to\lxmonika\lxmonika.sys --core path\to\lxstub\LXCORE.sys
```

## FAQ

### Is this a reimplementation of Microsoft's `lxcore.sys`?

**No**.

This `lxcore.sys` replacement stub only implements a stubbed `LxInitialize` function, which is
required for `lxmonika.sys` to compile and link successfully, as the WDK does not provide export
`.lib`s for `lxcore.sys`.

At runtime, `LxInitialize` is expected to be provided by the "real" `lxcore.sys` present on the
system. If this stub is installed instead, no Linux subsystem functionality will be available.

Future ports of the full Linux kernel on top of `lxmonika` are planned, but they will not be
delivered here.

### Why does `lxmonika.sys` link to `lxcore.sys` in the first place?

As part of its [load strategy](../lxmonika/README.md#new-loading-technique), `lxmonika.sys`,
in many cases, will replace either `lxss.sys` or `adss.sys`. The driver therefore takes on the
responsibility of loading and initializing `lxcore.sys` to ensure normal subsystem functionality.

Since `lxmonika.sys` is designed to load very early at boot time, most of the filesystem is not
available during initialization. This includes `\SystemRoot\System32\drivers`, where most of the
drivers are installed. Therefore, dynamic loading through `ZwLoadDriver` or `MmLoadSystemImage`
will not work. `lxmonika.sys` has to statically specify `lxcore.sys` in its PE metadata to
instruct the bootloader to preload the LX core before giving control to the kernel.

## Community

This repo is a part of [Project Reality](https://discord.gg/bcV3gXGtsJ).

Need help using this project? Join me on [Discord](https://discord.gg/bcV3gXGtsJ), and let's find a
solution together.
