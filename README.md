# Linux Monitor Kernel Access (Just Monika)

[![Discord Invite](https://dcbadge.vercel.app/api/server/bcV3gXGtsJ?style=flat)](https://discord.gg/bcV3gXGtsJ)

Developed by [trungnt2910](https://trungnt2910.com/).

This repository, "Linux Monitor Kernel Access" (also known as "Just Monika"), houses a collection of tools and drivers related to monitoring and interacting with the Windows Pico process infrastructure, primarily focusing on WSL1 (Windows Subsystem for Linux).

## Overview

The project explores advanced techniques for intercepting and modifying kernel-level interactions of Pico processes. This includes functionalities like altering syscall return values and potentially extending or fixing aspects of WSL1.

This repository is structured into several sub-projects, each with its own specific focus and detailed documentation. Please refer to the README files within the respective subfolders for more information:

*   `lxmonika/`: The core driver for monitoring Pico provider callbacks.
*   `lxstub/`: A stub component related to `lxmonika`.
*   `monika/`: A command-line interface for managing `lxmonika`.
*   `mxss/`: An example of a custom Pico provider.

<!-- Original comments below for posterity -->
<!-- Not to be confused with HyClone's Haiku syscall translation layer,
[monika](https://github.com/trungnt2910/hyclone/tree/master/monika). -->

<!-- I swear this repo has no reference to games/films/other popular culture. -->
