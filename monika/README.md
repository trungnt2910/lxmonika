# monika.exe

[![Discord Invite](https://dcbadge.vercel.app/api/server/bcV3gXGtsJ?style=flat)](https://discord.gg/bcV3gXGtsJ)

Unified host for `lxmonika`.

## Overview

`monika.exe` provides a command line interface to manage `lxmonika` and `lxmonika`-based Pico
providers.

## Building

Build this as a normal Visual Studio project.

## Installation

The build output at `$(SolutionDir)\monika\bin\$(Configuration)\$(Platform)\monika.exe` can be run
as a standalone executable file. No installation is required.

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

This repo is a part of [Project Reality](https://discord.gg/bcV3gXGtsJ).

Need help using this project? Join me on [Discord](https://discord.gg/bcV3gXGtsJ), and let's find a
solution together.
