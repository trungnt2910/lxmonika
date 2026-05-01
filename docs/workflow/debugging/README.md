# lxmonika Debugging Tips

[![Discord Invite][2]][1]

These are tips and tricks to help identifying issues with `lxmonika`.

## Debugging

The recommended tool for `lxmonika` debugging is
[WinDbg](https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/windbg-overview).

Both Clang and MSVC builds provide PDB files for every `lxmonika` component.

### Symbols

PDB symbols can be found at `out/${Configuration}/Debug/${Architecture}`.

For binaries installed from NuGet packages, you can download and extract the `.nupkg` file.
Symbols can be found in the `c/Debug/${Architecture}` folder.

### QEMU

If you are emulating Windows with QEMU (usually for ARM/ARM64 builds), `KDNET` (kernel debugging
through the network) will not work.

Instead, you may want to use serial debugging, expose the serial port as a TCP port, and use the
[TcpToPipe](https://github.com/trungnt2910/TcpToPipe) tool to forward the connection to a Windows
named pipe.

## Event Tracing

On some targets, such as bare-metal ARM devices, it may be very difficult to attach kernel debuggers
and obtain logs.

Fortunately, `lxmonika` emits Event Tracing logs in addition to Windows debugging logs, which can be
captured during the boot process. We provide the `lxmonika`-specific trace configuration file at
[`lxmonika/lxmonika.wprp`](/lxmonika/lxmonika.wprp).

To enable boot logging, run:

```cmd
wpr -boottrace -addboot lxmonika\lxmonika.wprp
```

Then reboot your device.

After restarting, run:

```cmd
wpr -boottrace -stopboot lxmonika.etl LXMONIKA_LOG
```

You can then open the resulting `.etl` file using the
[Windows Performance Analyzer](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-analyzer).

## Sanitizers

Clang-based Debug builds of the userland component (i.e. `monika.exe`) come with ASan and UBSan
enabled for supported architectures.

## Community

This repo is a part of [Project Reality][1].

Need help using this project? Join me on [Discord][1], and let's find a solution together.

[1]: https://reality.trungnt2910.com/discord/lxmonika
[2]: https://img.shields.io/discord/1185622479436251227?logo=discord&logoColor=white&label=Discord&labelColor=%235865F2
