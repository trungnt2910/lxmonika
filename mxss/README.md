# mxss - Windows Subsystem for Monix

[![Discord Invite](https://dcbadge.vercel.app/api/server/bcV3gXGtsJ?style=flat)](https://discord.gg/bcV3gXGtsJ)

A proof-of-concept driver for [`lxmonika`](../lxmonika).

## About Monix

Monix is a fictional Unix-like operating system.

It is inspired by our partner project, [SysX](https://github.com/itsmevjnk/sysx).
Like SysX, it is still under development and only supports a few simple functions.

## Installation

### Building the `mxss` driver

Please refer to the [`lxmonika`](../lxmonika/README.md#build-instructions) build instructions to
work out how to build and install a Windows kernel driver. For experienced NT driver devs, your
favorite workflow should be good.

You will have to install both `lxmonika` and `mxss` for the driver to function properly.

### Building the Monix distro

On a Linux machine (WSL1 should work), run:

```sh
cd /repo/root/mxss/monix
./build.sh $ARCH
```

Replace `$ARCH` with the architecture of the target machine. Currently only `x86_64` is supported.

A C++23 compiler is required. The latest of `g++` or `clang++` should suffice.

After building, a tarball should appear in `/repo/root/mxss/monix/bin/$ARCH`. Copy that tarball
to your target machine, and then run:

```cmd
wsl --import Monix C:\Path\To\Install\Monix C:\Path\To\Tarball\monix-wsl-%ARCH%.tar.gz --version 1
```

This should install Monix as a WSL1 distro. To run it:

```cmd
wsl -d Monix
```

Something like this should appear:

```cmd
C:\Users\trung>wsl -d Monix
Bootstrapping the container with Monix loader...
Hello, Monix World!
Monix container exited.
```

## Community

This repo is a part of [Project Reality](https://discord.gg/bcV3gXGtsJ).

Need help using this project? Join me on [Discord](https://discord.gg/bcV3gXGtsJ), and let's find a
solution together.
