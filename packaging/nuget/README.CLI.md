# monika

[![Discord Invite][2]][1]

`lxmonika` Framework

## Overview

`lxmonika` is a framework for building and managing Windows Pico process providers.

This package provides:
- `monika.exe`, a command line interface to manage `lxmonika` and `lxmonika`-based Pico providers.
- `lxmonika.sys`, the core kernel-mode component for `lxmonika`.
- Debug symbol (`.pdb`) files for these components.

To build your own Pico provider, use the [`monika.SDK`](https://www.nuget.org/packages/monika.SDK)
NuGet package.

## Installation

`monika.exe` is packaged as a
[.NET tool](https://learn.microsoft.com/en-us/dotnet/core/tools/global-tools).

To install, run:

```cmd
dotnet tool install --global monika
```

For Debug builds with additional symbols and runtime checks, use the
[`monika.Debug`](https://www.nuget.org/packages/monika.Debug) package.

```cmd
dotnet tool install --global monika.Debug
```

## Usage

`monika.exe` requires Administrator rights for most of its operations.

Run `monika --help` to get detailed help for all available commands.

Some common use cases:
```cmd
rem Install lxmonika (bundled with this package)
monika install

rem Install a specific build of lxmonika
monika install path\to\lxmonika.sys

rem Uninstall lxmonika
monika uninstall

rem Display lxmonika status
monika --info

rem Install a lxmonika provider
monika install provider path\to\provider\example.sys

rem Uninstall a lxmonika provider
monika uninstall provider example

rem Execute a Pico process for a lxmonika provider
monika exec --provider ExampleOS --cd "path\to\binary\dir" binary
```

## Community

This package is a part of [Project Reality][1].

Need help using this project? Join me on [Discord][1], and let's find a solution together.

[1]: https://reality.trungnt2910.com/discord/lxmonika
[2]: https://img.shields.io/discord/1185622479436251227?logo=discord&logoColor=white&label=Discord&labelColor=%235865F2
