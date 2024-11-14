# mxss - Windows Subsystem for Monix

[![Discord Invite](https://dcbadge.vercel.app/api/server/bcV3gXGtsJ?style=flat)](https://discord.gg/bcV3gXGtsJ)

A proof-of-concept driver for [`lxmonika`](../lxmonika).

## About Monix

Monix is a fictional Unix-like operating system.

It is inspired by our partner project, [SysX](https://github.com/itsmevjnk/sysx).
Like SysX, it is still under development and only supports a few simple functions.

## Installation

### Building the `mxss` driver

`mxss` requires [`lxmonika`](../lxmonika/README.md#build-instructions) to be built and installed.

After that, the [`monika.exe`](../monika) CLI can be used to deploy `mxss`.
```cmd
monika.exe install provider path\to\mxss\mxss.sys
```

### Building the Monix distro

On a Linux machine (WSL1 should work), run:

```sh
cd /repo/root/mxss/monix
./build.sh $ARCH
```

Replace `$ARCH` with the architecture of the target machine. Currently `x86_64` and `aarch64` are
supported.

A C++23 compiler is required. The latest of `g++` or `clang++` should suffice.

After building, a tarball should appear in `/repo/root/mxss/monix/bin/$ARCH`. Copy that tarball
to your target machine, and extract it anywhere.

### Building the Windows Subsystem for Monix host (`mxhost`)

Build the `mxhost` project. This should produce a normal Win32 executable at
`$(SolutionDir)\mxss\mxhost\bin\$(Configuration)\$(Platform)\mxhost.exe`. Copy it to your target
machine.

### Running

#### Dedicated Monix host

On your target machine, run:

```cmd
\path\to\mxhost.exe \path\to\extracted\monix\root
```

Something like this should appear:
```
C:\Users\trung\Desktop>mxhost\mxhost mxhost
[INF] D:\CodingProjects\lxmonika\mxss\mxhost\mxhost.cpp:116: Monix version 0.0.1 prealpha (compiled Dec 28 2023 20:36:20)
[INF] D:\CodingProjects\lxmonika\mxss\mxhost\mxhost.cpp:117: Copyright <C> 2023 Trung Nguyen (trungnt2910)
[INF] D:\CodingProjects\lxmonika\mxss\mxhost\mxhost.cpp:119: initializing kernel device
[INF] D:\CodingProjects\lxmonika\mxss\mxhost\mxhost.cpp:158: initializing system root
Available binaries:
hello.
>
```

You can then use Monix like an authentic [SysX](https://itsmevjnk.github.io/sysx-build) system.

#### Unified `monika.exe` host

The `monika.exe` CLI can also be used to launch a Monix process.
```cmd
monika exec --provider Monix --cd "\path\to\extracted\monix\root\bin" hello
```

## Community

This repo is a part of [Project Reality](https://discord.gg/bcV3gXGtsJ).

Need help using this project? Join me on [Discord](https://discord.gg/bcV3gXGtsJ), and let's find a
solution together.
