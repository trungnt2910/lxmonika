# lxmonika CI

[![Discord Invite][2]][1]

`lxmonika` uses GitHub Actions for Continuous Integration (CI).

## Overview

`lxmonika` currently maintains a build workflow at
[`.github/workflows/build.yml`](/.github/workflows/build.yml).

On every push to development branches and pull requests to `master`, the workflow:
- [Builds](/docs/workflow/building/README.md) `lxmonika`.
  + For better coverage, both CMake builds and
  [Legacy MSBuild builds](/docs/workflow/building/legacy-msbuild.md) are supported.
- [Tests](/docs/workflow/testing/README.md) `lxmonika`.
- Packages `lxmonika`.
  + Packages will be uploaded as artifacts for every run.
  + For runs on `master`, NuGet packages will also be uploaded.

## Roadmap

We are continuously working to improve `lxmonika`'s CI. This includes:
+ Reducing SDK installation times.
+ Running automated tests.
+ Improving stability.
  + Currently, flaky setup scripts and GitHub's unreliability may cause CI failures.

## Community

This repo is a part of [Project Reality][1].

Need help using this project? Join me on [Discord][1], and let's find a solution together.

[1]: https://reality.trungnt2910.com/discord/lxmonika
[2]: https://img.shields.io/discord/1185622479436251227?logo=discord&logoColor=white&label=Discord&labelColor=%235865F2
