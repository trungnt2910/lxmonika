# lxmonika Coding Guidelines

[![Discord Invite][2]][1]

This is a non-exhaustive list that is subject to change.

The rule of thumb is to follow the surrounding code.

## C++

- Fine-tune your code for MSVC.
  + Prefer using the latest [C++ features supported by MSVC][3].
    - At the time of writing, MSVC has good C++23 support but not C++26.
  + MSVC extensions are allowed. Do not use Clang/GNU extensions.
  + If Clang does not support something correctly, [implement it][4].
    - Or report it in our [LLVM features tracking issue][5].
- Avoid macros.
  + Macro values should be replaced by `constexpr` variables.
  + Macro functions should be replaced by `inline` functions.
  + Macros are still preferred to code duplication.
- Don't repeat yourself.
  + Split recurring patterns into inline functions whenever possible.
  + Macros may be used for patterns that cannot be split into functions. For example:
    - Local variable declarations.
    - Return statements.
- Use `AUTO_RESOURCE` or other RAII mechanisms suitable for the project.
- `constexpr` all the things.
  + Static data structures (strings, callback tables, etc.) should be initialized at compile time.
  + Flag conversion/validation should be done at compile time when possible.
- Architecture-specific code should be separated.

[3]: https://learn.microsoft.com/en-us/cpp/overview/visual-cpp-language-conformance
[4]: https://github.com/llvm/llvm-project/pull/185282
[5]: https://github.com/trungnt2910/lxmonika/issues/10

## Community

This repo is a part of [Project Reality][1].

Need help using this project? Join me on [Discord][1], and let's find a solution together.

[1]: https://reality.trungnt2910.com/discord/lxmonika
[2]: https://img.shields.io/discord/1185622479436251227?logo=discord&logoColor=white&label=Discord&labelColor=%235865F2
