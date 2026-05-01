# lxmonika Build Instructions

[![Discord Invite][2]][1]

## Overview

`lxmonika` is built primarily with CMake and the Clang toolchain.

The following instructions cover CMake-based builds. For Visual Studio/MSBuild builds, refer to
[the legacy MSBuild instructions](./legacy-msbuild.md).

## Prerequisites

### Build Tools

To build `lxmonika`, you will need:

- CMake
- Ninja
- Windows SDK & WDK
  - The latest
  [SDK/WDK](https://learn.microsoft.com/en-us/windows-hardware/drivers/install-the-wdk-using-winget#step-2-install-windows-sdk-and-wdk)
  is preferred.
  - SDK/WDK version 22621 should also be installed for 32-bit (x86, ARM) support.

To install these, you can run:

```cmd
winget install --id Kitware.CMake
winget install --id Ninja-build.Ninja
:: Need a dummy log file to prevent winget from failing with a weird error on 22621 SDK.
winget install --id Microsoft.WindowsSDK.10.0.22621 --log %TEMP%\sdk-install.log
winget install --id Microsoft.WindowsWDK.10.0.22621
winget install --id Microsoft.WindowsSDK.10.0.26100
winget install --id Microsoft.WindowsWDK.10.0.26100
```

You will also need the LLVM toolchain for MinGW. A compatible toolchain is automatically downloaded
by the build script.

### IDE

You can build `lxmonika` entirely from the command line.

However, Visual Studio Code with the
[CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension
is highly recommended. Most CMake commands in this guide can be done in the VSCode UI.

### WDK Test Signing Certificate

If you have not built any WDK projects on the current device, you may need to create a WDK test
signing certificate.

To check if you already have a certificate generated, from a PowerShell session, run:

```powershell
Get-ChildItem -Path Cert:\CurrentUser\My | Where-Object { $_.Subject -like "*WDKTestCert*" }
```

If you have no certificates listed, you may have to create one. From a PowerShell session, run:

```powershell
New-SelfSignedCertificate                       `
    -Subject "CN=WDKTestCert"                   `
    -CertStoreLocation "Cert:\CurrentUser\My"   `
    -Type CodeSigning
```

## Building

### TL;DR

To build core `lxmonika` features for `x64` devices, run:

```cmd
cmake --preset Debug-x64
cmake --build bin/Debug/x64 --target install
```

Continue reading if you want to build for other targets or further customize your build.

### Presets

`lxmonika`'s CMake build system offers different _presets_, which are similar to MSBuild
configurations. A preset consists of two parts:

1. Configuration:
- Debug: Builds optimized for development and debugging.
- Release: Builds optimized for performance.
2. Archiecture: One of the four supported architectures: `x86`, `x64`, `ARM`, `ARM64`.

For example, to build in Release mode for 32-bit ARM, set the preset to `Release-ARM`.

The chosen preset should be passed in the `--preset` option when running the configuration step.

If you are using Visual Studio Code, you can also choose your preset by pressing Ctrl + Shift + P
and running the "CMake: Select Configure Preset" command.

### Build Directory

By default, the build directory is determined by the preset. It will be:

```
bin/${Configuration}/${Architecture}
```

To build, run `ninja` in the build directory, or pass the path to `cmake --build`:

For example, to build for `Debug-x64`:

```
cmake --build bin/Debug/x64
```

### Output Directory

When you run `cmake --install`, the build artifacts will be copied to the output directory, which
is:

```
bin/${Configuration}/${Architecture}
```

### Features

Features are targets containing a related subset of the project. There are currently two features:

1. `Core`: `lxmonika` core components. Includes the `lxmonika` driver and `monika.exe` CLI.
2. `MXSS`: Includes the proof-of-concept [Windows Subsystem for Monix](/mxss/README.md).

By default, the whole project is built, but only `Core` is installed and packaged. You can set
`MONIKA_INSTALL_MXSS=ON` to also install `MXSS`.

The target name for a feature is `Feature${NAME}` (e.g. `FeatureCore`, `FeatureMXSS`).

For example, to build MXSS `Debug-x64`:

```cmd
cmake --build bin/Debug/x64 --target FeatureMXSS
```

### Additional Options

#### Custom Signing Certificate

By default, `lxmonika` looks for certificates issued to `WDKTestCert` to sign drivers.

If you wish to sign with a different certificate, you can specify the subject name in
`MONIKA_CERT_NAME` during configuration:

```cmd
cmake --preset Debug-x64 -DMONIKA_CERT_NAME="JustMonika"
```

#### Including/Excluding Features

By default, `Core` is included, but not `MXSS`.

If you wish to include/exclude specific [features](#features), you can specify
`MONIKA_INSTALL_${FEATURE}=${OFF_OR_ON}` during configuration.

For example, to disable installing `Core` (note that this will affect [packing](#packing)):

```cmd
cmake -DMONIKA_INSTALL_CORE=OFF
```

## Installing

To install `lxmonika`, first, run CMake with `--install` to organize the artifacts in the
[output directory](#output-directory):

For example, to install for `x64-Debug`:

```cmake
cmake --install bin/Debug/x64
```

If you are installing `lxmonika` on a remote machine, copy the contents of the `out` directory to
the target.

You can then use the `monika.exe` CLI
(located at `out/${Configuration}/bin/${Architecture}/monika.exe`) to install `lxmonika` or Pico
providers on the target machine.

For example, to install the newly built `x64-Debug` `lxmonika.sys`:

```cmd
out/Debug/bin/x64/monika.exe install
```

For more details on the `monika.exe` tool, please refer to
[the `monika.exe` project](/monika/README.md).

### Test Mode

If you have not already enabled
[Test Mode](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/test-signing),
you should do so on your remote machine.

On an elevated Command Prompt window on the test computer:

- Enable test signing.
```cmd
bcdedit /set testsigning on
```
- Reboot the device.
```cmd
shutdown /r /t 00
```

### Uninstalling

To uninstall `lxmonika`, simply run `monika.exe uninstall` and reboot. You may also need to
uninstall any dependent Pico provider by running `monika.exe uninstall provider` first.

## Packing

### NuGet

`lxmonika` supports installation using NuGet packages. This applies to both the `lxmonika` SDK and
the `monika.exe` CLI.

To build a full NuGet package for a certain configuration (Debug/Release), you will need to
separately run CMake build & install for all supported architectures:

```cmd
cmake --preset Debug-x64 && cmake --build bin/Debug/x64 --target install
cmake --preset Debug-x86 && cmake --build bin/Debug/x86 --target install
cmake --preset Debug-ARM64 && cmake --build bin/Debug/ARM64 --target install
cmake --preset Debug-ARM && cmake --build bin/Debug/ARM --target install
```

The [output directory](#output-directory) will contain `.nuspec` definition files at
`out/${Configuration}`. You can use these files with `nuget pack`:

```cmd
:: monika.exe CLI
nuget pack out/Debug/monika.nuspec
:: lxmonika SDK
nuget pack out/Debug/monika.SDK.nuspec
```

## Community

This repo is a part of [Project Reality][1].

Need help using this project? Join me on [Discord][1], and let's find a solution together.

[1]: https://reality.trungnt2910.com/discord/lxmonika
[2]: https://img.shields.io/discord/1185622479436251227?logo=discord&logoColor=white&label=Discord&labelColor=%235865F2
