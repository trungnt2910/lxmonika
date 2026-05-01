# lxmonika Workflow

[![Discord Invite][2]][1]

This section documents the recommended workflows for building, debugging, and testing `lxmonika`.

## Building

`lxmonika` is built primarily with CMake and the Clang toolchain. You can follow
[these instructions][3] to build with CMake.

To ensure compatibility with the rest of the ecosystem, we also maintain a legacy build system based
on Visual Studio, MSBuild, and MSVC. You can follow [these instructions][4] to build with MSBuild.

[3]: building/README.md
[4]: building/legacy-msbuild.md

## Debugging

Check the [debugging instructions][5] for tips on how to effectively debug `lxmonika` in different
scenarios.

[5]: debugging/README.md

## Testing

`lxmonika`'s quality is ensured by a set of automated tests and occasional manual testing. You can
find out how to run tests and the project's testing strategy [here][6].

[6]: testing/README.md

## Community

This repo is a part of [Project Reality][1].

Need help using this project? Join me on [Discord][1], and let's find a solution together.

[1]: https://reality.trungnt2910.com/discord/lxmonika
[2]: https://img.shields.io/discord/1185622479436251227?logo=discord&logoColor=white&label=Discord&labelColor=%235865F2
