# lxmonika

[![Discord Invite](https://dcbadge.vercel.app/api/server/bcV3gXGtsJ?style=flat)](https://discord.gg/bcV3gXGtsJ)

A Windows driver to monitor kernel access of Pico processes.

## Current functionality

`lxmonika` intercepts every Pico provider callbacks and forwards the calls to `lxcore`.

If `lxmonika` encounters any WSL1 process making the `uname` syscall, it will modify the returned
structure. Specifically:

- `sysname` will become `Monika` (instead of `Linux`).
- `release` will be the latest LTS release of the Linux kernel (currently `6.6.0`).
- `version` will be the build version of `lxmonika` instead of a hard-coded string defined by
`lxcore`.

`lxmonika` currently does not modify any other interactions of WSL1 processes with the kernel.

![Just Monika](monika.png)

## How it works

### New loading technique

The current versions of `lxmonika` work by directly calling `PsRegisterPicoProvider`, an
undocumented function used by `lxcore`, to install itself as a Pico provider. `lxmonika`
detects the structure sizes that the function requires, allowing drivers to work seamlessly across
breaking changes throughout Windows's version history.

To be able to call `PsRegisterPicoProvider`, `lxmonika` needs to be loaded as a "core" driver, or
one specified by a "core" service whitelisted by the bootloader (`winload`). To achieve this,
`lxmonika` "borrows" one of these services. On machines with WSL1 (most 64-bit x86 and ARM
Windows), the borrowed service is `LXSS`, on machines with AoW/Project Astoria, it is `ADSS`, and
on other Windows 10 versions, it is `CNG`. The driver for the borrowed service will be replaced
with `lxmonika.sys`, and `lxmonika.sys` will be responsible for bootstrapping the actual driver.

`lxmonika` is linked to `lxcore.sys` and always attempts to load `lxcore`. Before loading,
`lxmonika` installs a temporary hook at `nt!PsRegisterPicoProvider` to its managed variant,
`lxmonika!MaRegisterPicoProvider`, allowing `lxcore` to be registered as a `lxmonika` provider and
function seamlessly like one. The hook also prevents `lxcore` from overriding callbacks already set
by `lxmonika`.

### Legacy heuristics

The legacy heuristics is used when `lxmonika` fails to be loaded as a core driver.

Instead of calling `PsRegisterPicoProvider`, `lxmonika` searches for the provider callbacks
structure (the one stored by this function) in memory and tries to synthesize the Pico callbacks
structure (the one returned by this function).

`lxmonika` first locates `ntoskrnl.exe` (the NT kernel image) and the `lxcore.sys` driver in
memory. Then, it uses heuristics to locate `PspPicoProviderRoutines`, a structure containing
functions the NT kernel will call whenever a WSL1 process makes interactions with the kernel, such
as performing a `syscall`.

The legacy method involves late kernel patching and may trigger PatchGuard. It is therefore
unsupported.

### Linux syscall hooking

`lxmonika` detects the `syscall` number from the syscall dispatch callback in a `PKTRAP_FRAME`
parameter. It then looks for `uname` and modifies the return values.

## Potential applications

While intercepting `uname` to bump up the kernel version without adding any features does not
seem useful (other than for showing off with `neofetch`), being able to intercept all WSL1
processes has huge implications.

Since Microsoft no longer supports WSL1 (The
["WSL1"](https://github.com/microsoft/WSL/issues?q=is%3Aopen+is%3Aissue+label%3Awsl1) label on
their tracker repo basically means "wontfix", based on the fact that only 11 out of more than 100
issues are closed, and some closed issues are marked "fixed-in-WSL2"), being able to intercept
syscalls means that third-party kernel drivers can potentially deliver their own fixes, given
enough knowledge about the NT kernel internals.

`lxmonika` also bridges between older `lxcore.sys` and newer `ntoskrnl.exe` with ABI changes,
allowing a part of Project Astoria to work on newer 32-bit Windows builds.

It is also possible for open-source projects to port the whole Linux kernel as a Windows driver,
creating a more modern version of [coLinux](http://www.colinux.org/) with 64-bit support and
better integration.

Finally, since Pico processes are not restricted to Linux, we can implement other kernels'
ABIs, such as Darwin.

<!-- HyClone on Windows when? -->

## Build instructions

### Prerequisites

- Visual Studio 2022.
- The latest WDK. Download it
[here](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk).
- WDK version
[10.0.14393.0](https://download.microsoft.com/download/8/1/6/816FE939-15C7-4185-9767-42ED05524A95/wdk/wdksetup.exe)
for 32-bit (x86, ARM) and x64 builds.
- WDK version
[10.0.17134.0](https://download.microsoft.com/download/B/5/8/B58D625D-17D6-47A8-B3D3-668670B6D1EB/wdk/wdksetup.exe)
for ARM64 builds.
- A Windows NT 10.0 machine with Test Signing enabled.

### Instructions

Please use the [`monika.exe`](../monika) CLI to deploy `lxmonika`. This specialized tool
handles "borrowing" and other quirks specific to `lxmonika.sys`, unlike generic tools like `sc`
or `pnputil`.

#### For the first time you deploy a driver on a new computer

On an elevated Command Prompt window on the test computer:

- Enable test signing.
```bat
bcdedit /set testsigning on
```
- Reboot the device.
```bat
shutdown /r /t 00
```

#### Every time you want to test

- Build this driver on Visual Studio 2022 (Right-click the project -> Build).
- Copy the output folder, located at
`$(SolutionDir)\lxmonika\bin\$(Configuration)\$(Platform)\lxmonika`, to the test device.

On an elevated Command Prompt window on the test computer:

- Uninstall existing installations of `lxmonika`.
```bat
monika uninstall
```
- Run the command below. Replace the `path\to` part with the relevant paths.
```bat
monika install path\to\lxmonika\lxmonika.sys
```
- Reboot the device.
```bat
shutdown /r /t 00
```

#### When you want to uninstall the driver

On an elevated Command Prompt window on the test computer:

- Run the command below.
```bat
monika uninstall
```
- Reboot the device.
```bat
shutdown /r /t 00
```

### Notes

The driver has been tested on some builds of Windows 10 and Windows 11 and on all supported
architectures (x86, x64, ARM, ARM64). Depending on the specific build you use, YMMV.

Windows 10 TH2 (all architectures) and Windows 10 pre-RS4 (ARM64) are not supported, since they do
not provide the required Pico processes infrastructure.

## Building your own Pico provider

Your driver will need to add a reference to `lxmonika`. It should also have headers in the
[`include/`](include) directory included.

```xml
    <ClCompile>
      <AdditionalIncludeDirectories>$(SolutionDir)\lxmonika\include;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
    <Link>
      <AdditionalLibraryDirectories>$(SolutionDir)\lxmonika\$(OutDir)</AdditionalLibraryDirectories>
      <AdditionalDependencies>%(AdditionalDependencies);lxmonika.lib</AdditionalDependencies>
    </Link>
```

After having the required imports, call `MaRegisterPicoProvider` or `MaRegisterPicoProviderEx` to
register as a Pico provider.

Install your driver using the `monika.exe` CLI.
```bat
monika install provider path\to\your\driver.sys
```

See the included sample, [`mxss`](../mxss), for more details.

## Future plans

- Port a newer version of Linux, Darwin, or even Haiku???
- (Far, far future) Revive project Astoria???

## Community

This repo is a part of [Project Reality](https://discord.gg/bcV3gXGtsJ).

Need help using this project? Join me on [Discord](https://discord.gg/bcV3gXGtsJ), and let's find a
solution together.

## Acknowledements

Thanks to Martin Hron ([**@thinkcz**](https://github.com/thinkcz)) for his
[pico-toolbox](https://github.com/thinkcz/pico-toolbox), which provides the required definitions
and a proof-of-concept for Pico process hacking.

Thanks to Bill Zissimopoulos ([**@billziss-gh**](https://github.com/billziss-gh)) for his
[lxdk](https://github.com/billziss-gh/lxdk), which documents the various symbols exposed by
`lxcore`, enabling the functionality of `/dev/reality`. Unfortunately, due to licensing issues,
his great project cannot be directly incorporated into `lxmonika`.

Thanks to Justine Tunney ([**@jart**](https://github.com/jart)) for providing me with the funds for
an ARM64 device. Without this, the ARM64 port is virtually impossible.

## Suggested readings

- Microsoft's
[Pico Process Overview](https://learn.microsoft.com/en-us/archive/blogs/wsl/pico-process-overview).
- Windows Internals 7th Edition, Part 1, Pages 68-70, 121-124.
- [The Linux kernel hidden inside Windows 10](https://github.com/ionescu007/lxss/blob/master/The%20Linux%20kernel%20hidden%20inside%20windows%2010.pdf)
by Alex Ionescu ([**@ionescu007**](https://github.com/ionescu007)).
- [Write a Hello World Windows Driver (KMDF)](https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/writing-a-very-small-kmdf--driver).
Highly recomended if you are new to Windows driver development like me.
