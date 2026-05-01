# lxmonika Testing

[![Discord Invite][2]][1]

`lxmonika`'s quality is ensured by a set of automated tests and occasional manual testing.

## Automated Testing

`lxmonika` currently lacks automated testing apart from the continuous build pipelines. We are
actively working towards making `lxmonika` more testable.

### Driver Unit Tests

We are experimenting with building a userland driver emulation framework for `lxmonika.sys`. The
draft test repository can be found at https://github.com/trungnt2910/BootstrapPlayground.

This framework loads `lxmonika.sys` fully in usermode, mocks `ntoskrnl.exe` exports, and gracefully
traps and emulates privileged instructions.

The framework can then be used alongside with a testing framework such as `gtest` for unit testing.

#### Architecture Coverage

Unit test binaries will be run under WINE and QEMU on Linux CI hosts.

This allows covering all supported architectures while minimizing costs compared to full Windows OS
emulation.

#### Compiler Coverage

The same unit tests will be run with a MSVC-based build and a Clang-based build to expose potential
compiler-related runtime bugs.

#### Code Coverage

Clang-based `Debug` test builds will be compiled with
[source-based code coverage](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html).

We aim to have high coverage for the most critical components, including `monika` initialization,
`reality`, `pico`, and the dispatcher.

### CLI Unit Tests

In the future, we may refactor the `monika.exe` CLI to be more mockable and testable by providing
a layer of abstraction for file and registry APIs.

### Integration Tests

Integration tests are difficult to build for `lxmonika` since installing kernel components requires
rebooting the test machine, which is not supported by GitHub Actions.

## Manual Testing

Manual testing is done by building and installing `lxmonika` on a wide variety of Windows versions
and architectures.

This allows end-to-end functionality verification on real hardware, something that CI machines
cannot provide.

For build and install instructions, refer to
[these instructions](/docs/workflow/building/README.md).

## Community

This repo is a part of [Project Reality][1].

Need help using this project? Join me on [Discord][1], and let's find a solution together.

[1]: https://reality.trungnt2910.com/discord/lxmonika
[2]: https://img.shields.io/discord/1185622479436251227?logo=discord&logoColor=white&label=Discord&labelColor=%235865F2
