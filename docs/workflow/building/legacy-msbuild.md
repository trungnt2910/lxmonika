# lxmonika Legacy MSBuild Builds

`lxmonika` was originally built using Visual Studio 2022/MSBuild/MSVC before migrating to
CMake/Clang.

To ensure compatibility with the Microsoft ecosystem, we still support workflows based on Visual
Studio. However, we highly recommend using the [CMake](./README.md)-based development workflow
instead.

## Prerequisites

- Visual Studio 2022. Visual Studio 2026 is **not** supported yet.
- Windows SDK/WDK version 10.0.22621.
- Windows SDK/WDK version 10.0.14393 for 32-bit (x86, ARM) and x64 builds.
- Windows SDK/WDK version 10.0.17134 for ARM64 builds.

To install required the Windows SDK/WDK versions and apply certain hacks for 32-bit platforms, you
can run the `./setup.ps1` script.

## Building

You can build by opening the `lxmonika.sln` solution in Visual Studio 2022.

`lxmonika` uses .NET-style output paths. Binary outputs are located at
`bin/$(Configuration)/$(Architecture)`, relative to the Visual Studio project path.


## Installation

`./pack.ps1` will pack all build outputs into the `out` directory, allowing the binaries to be
copied to the target machine.

## Limitations

While Visual Studio/MSBuild builds will be supported in the long run, we plan to drop support for
targeting legacy platforms using MSBuild-based binaries.

This is due to the various breaking changes Microsoft makes with every release of Visual Studio and
WDK, including removing support for 32-bit platforms.

For more portable builds on top of open-source technology, we recommend using the
[CMake](./README.md)-based development workflow.
