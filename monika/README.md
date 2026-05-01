# monika.exe

[![Discord Invite][2]][1]

Unified host for `lxmonika`.

## Overview

`monika.exe` provides a command line interface to manage `lxmonika` and `lxmonika`-based Pico
providers.

## Building

To build and install `monika.exe`, please follow
[these instructions](/docs/workflow/building/README.md).

`monika.exe` is included with `FeatureCore` in CMake and with the `monika` project in
Visual Studio/MSBuild.

## Usage

`monika.exe` requires Administrator rights for most of its operations.

Run `monika --help` to get detailed help for all available commands.

Some common use cases:
```cmd
rem Install lxmonika
monika install path\to\lxmonika\lxmonika.sys

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

This repo is a part of [Project Reality][1].

Need help using this project? Join me on [Discord][1], and let's find a solution together.

[1]: https://reality.trungnt2910.com/discord/lxmonika
[2]: https://img.shields.io/discord/1185622479436251227?logo=discord&logoColor=white&label=Discord&labelColor=%235865F2
