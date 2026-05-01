# Internals

[![Discord Invite][2]][1]

This section documents the internals of `lxmonika` and related Windows NT functions.

## lxmonika

- [Overview](lxmonika/README.md)
  + Offset database and fallback heuristics.
  + Pico provider registration.
  + Dispatcher (Process/thread creation/termination, process/thread contexts, "parent" providers).
  + Interactions with `lxcore` (WSL & Project Astoria).
  + `/dev/reality` and `\Device\Reality`.
  + Transparency & compatibility issues.
  + Additional Pico callbacks.
  + Loading process & Borrowing.

## Windows NT

- [Overview](windows-nt/README.md)
  + `\Device\ConDrv\KernelConnect`.
  + Pico process/thread lifetime (process switching on `exec`, thread cleanup using APCs, etc...).
  + `MEM_DOS_LIM` and Pico processes.
  + How Core Boot drivers are loaded.
  + Pico API/ABI changes throughout the ages.

## Community

This repo is a part of [Project Reality][1].

Need help using this project? Join me on [Discord][1], and let's find a solution together.

[1]: https://reality.trungnt2910.com/discord/lxmonika
[2]: https://img.shields.io/discord/1185622479436251227?logo=discord&logoColor=white&label=Discord&labelColor=%235865F2
