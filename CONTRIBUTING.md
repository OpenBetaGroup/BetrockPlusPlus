# Contributing

Simply fork the [`main` branch](https://github.com/OpenBetaGroup/BetrockPlusPlus/tree/main) and commit whatever changes you want to make.

Ensure your fork is up-to-date with the `main` branch, then make a pull request. Keep your changes localized to what needs to be changed to reduce the chance of merge conflicts!

## AI policy

Generally, we're not opposed to adding code that was created with the help of AI assistance to our codebase,
so long as the following rules are followed.

- The human submitter is responsible for:
  - Reviewing all AI-generated or assisted code
  - Ensuring compliance with licensing requirements
  - Taking full responsibility for the contribution
- The AI agent/assistant is forced to:
  - Check for functionality it wants to implement before implementing it, to avoid duplicated code
  - Keep its changes as minimal as possible, as to avoid overruning the codebase with unmanagable changes
- The AI agent/assistant is strictly forbidden from:
  - Breaking existing functionality
  - Writing comments
  - Remove unsolved TODOs
  - Remove existing comments
  - Remove NOTEs
  - Change formatting guidelines
  - Ignore formatting guidelines
  - Adding Signed-off-by or Co-authored-by tags
  - Writing commit text
  - Writing pull request text
- Proper marking of LLM generated content
  - Small sections of code (1-3 lines) can remain unattributed, as such a small change is unlikely to cause issues, especially if properly reviewed. Additionally these would quickly clog up the code with the `LLM disclosure` tags
  - Larger changes (function-additions, full rewrites, refactors, etc.) need to be marked as follows:

```text
// LLM disclosure
<new code>
// End LLM disclosure
```

This project is supposed to be a fun learning-exercise for whoever wants to get involved, and to foster a better understanding of how Beta works. This project doesn't need to be finished overnight in a half-broken state that nobody understands.

_These guidelines are inspired by the [docs.kernel.org page for coding assistants](https://docs.kernel.org/process/coding-assistants.html) (as of 21. Oct, 2026)._

## Development

Grab the `main` branch for the most up-to-date, albeit unstable, repository.

## Recommended VSCode Extensions

- `clangd` (ideally `clangd-22` or later for Doxygen comment integration)
- CMake Tools
- Doxygen Documentation Generator

## Code Style Guide

The following covers the most important parts on how we style our code. These aren't super hard rules to follow, and most are automatically enforced by the formatter anyways.

### File structure

- File and directory names are in `snake_case`
- We use `LF`-only line endings
- Code files are `.cpp`, Headers are `.h`. While unconventional, its done so headers stick out better among the `.cpp` files. They are **NOT** C-header files!
- **Include the copyright notice at the top!** This also serves as a nice way for people to get credited, if the git history is ever lost. _**Any change**_ will get you added to there (within reason). If one already exists, be sure to add your own name _after_ those who came before you.

Use the template below if you're creating a new page.

```cpp
/*
 * Copyright (c) [year], [name] <temp@example.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
```

- Unless this is already obvious via its name or usage, **provide a short description what a file is for** the relevant file, why it exists and what its used for
- **Avoid keeping unused includes**. If an included thing is used inside of a `.cpp` file instead of the `.h` file it's included in, please move it out of the `.h` file and into the `.cpp` file!
- Run `run-clang-tidy` and `clang-format` over the files you changed

```bash
run-clang-tidy -fix
clang-format -i $(find ./src -name "*.cpp" -o -name "*.h")
```

### General

- Avoid old-style casts, e.g. `(int)5.0f` -> `int(5.0f)`. Using `static_cast<int>(5.0f)` is fine too

### Variables

- Variables are written in `camelCase`
- `#define` blocks and `constexpr` are written in all uppercase snake case, aka `SCREAMING_SNAKE_CASE`
- Avoid **ambiguous abbreviations**. Something like `pos` obviously means position, but just `p` or `m` could be anything. In for-loops or similar these are fine as iterators though
- **Immutable variables** should be marked as `const`
- **Magic numbers** should be turned into `#define` blocks or `constexpr` variables if they only belong to a specific class, struct or function
- Use types that guarantee a **known bit-width** (i.e. `int32_t`, `int16_t` or `int8_t`), unless you need the variable to be architecture dependent, or it's required by an external library that doesn't follow our rules to work
- Enums **must** have a type
- It's often smart to make enums into **enum classes**, as to avoid arbitrary associations with specific numbers or values
- **Parameters** are prefixed with an underscore, e.g. `_pos`, `_entity`

### Functions

- Functions are written in `PascalCase`
- Make use of **existing functionality**. Don't reinvent the wheel. Use what exists!
- While we are using C++, **avoid using classes** unless necessary or if it results in much cleaner code. Thanks to C++, structs can provide a lot of similar capabilities.
- If possible, **do not pass values separately**. Make use of structs that combine them. For example, instead of `int32_t posX, int32_t posY, int32_t posZ`, just use `Int3 pos`
- Only **pass by reference if it makes sense**. A `bool` can be passed by value. If a set of values are always together, turn them into a struct, then pass that struct of values.
- **Order variables** in structs from largest to smallest, as to reduce padding
- Combine bools via **bitfields**

```cpp
// (2 Bytes)
bool value1;
bool value2;
// (1 Byte / 2 Bits)
bool value1 : 1;
bool value2 : 1;
```
