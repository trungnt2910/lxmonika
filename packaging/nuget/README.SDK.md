# monika SDK

[![Discord Invite][2]][1]

`lxmonika` SDK.

## Overview

This package provides an SDK for Windows Kernel-mode drivers to create and manage Pico processes
using the `lxmonika` framework.

The package includes:
- Public headers for `lxmonika`, which exports undocumented Windows NT APIs along with some
`lxmonika` extensions.
- Glue libraries for `lxmonika`.

To install `lxmonika` and deploy your Pico provider, install the
[`monika`](https://www.nuget.org/packages/monika) package as a
[.NET tool](https://learn.microsoft.com/en-us/dotnet/core/tools/global-tools). You may also `unzip`
the provided `.nupkg` file and run `c\bin\$(Platform)\monika.exe` directly.

## Installation

The `lxmonika` SDK is packaged as a
[Native C++ NuGet package](https://learn.microsoft.com/en-us/nuget/consume-packages/finding-and-choosing-packages#native-c-packages).

To install, right-click on the Visual Studio project and choose "Manage NuGet Packages".

## Usage

Once the package is installed, you may consume `lxmonika` APIs:

```cpp
#include <lxmonika/monika.h>

extern "C"
NTSTATUS DriverEntry(/* Args */)
{
    MaRegisterPicoProvider(/* Params */);
}
```

You can deploy your Pico provider using the `monika.exe` CLI, which is included in the
[`monika`](https://www.nuget.org/packages/monika) package:

```cmd
monika install provider path\to\your\driver.sys
```

See the [`mxss`](https://github.com/trungnt2910/lxmonika/blob/master/mxss/README.md) example for how
to build `lxmonika`-based Pico providers.

## Community

This package is a part of [Project Reality][1].

Need help using this project? Join me on [Discord][1], and let's find a solution together.

[1]: https://reality.trungnt2910.com/discord/lxmonika
[2]: https://img.shields.io/discord/1185622479436251227?logo=discord&logoColor=white&label=Discord&labelColor=%235865F2
