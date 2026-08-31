# Gold Documentation

Gold is a C++20/26 data-driven framework with JS/TS-style ergonomics. Core concepts:
* `var` – universal dynamic value container
* `object` / `list` – RAII containers with prototype inheritance
* `goldjs.hpp` – JS-style sugar: `jo(...)` / `ja(...)`, `v["key"]`, template strings
* `gold::lang` – TypeScript-like scripting whose runtime is gold data
* Pluggable backends: window, input, audio, render, dataStore
* Web module: Express-like HTTP/WebSocket server, HTML5 rendering, file-based dataStore
* Game module: component-based engine, bgfx rendering, SDL3 window/input

## Quick start
```bash
git submodule update --init --recursive
cmake -S . -B build -DGOLD_BUILD_GAME=OFF -DGOLD_BUILD_WEB=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

## Docs
- [Core Types](core/types.md)
- [var / object / list](core/var_object_list.md)
- [goldjs sugar](core/goldjs.md)
- [Serialization](core/serialization.md)
- [gold::lang scripting](lang/scripting.md)
- [Web module](web/overview.md)
- [Game module](game/overview.md)
- [Backends](backends/overview.md)
- [Build & config](build.md)

## Conventions
See `AGENTS.md` for JS-style ergonomics, backend registration pattern, and gold-data-native events.

<!-- 
Agent note: This index is the entry point for developers and AI agents. Keep links in sync with files created by subagents. Update when new modules are added.
-->
