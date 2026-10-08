# Gold Documentation

Gold is a C++20/26 data-driven framework with JS/TS-style ergonomics. Core concepts:
* `var` – universal dynamic value container
* `object` / `list` – RAII containers with prototype inheritance
* `goldjs.hpp` – JS-style sugar: `jo(...)` / `ja(...)`, `v["key"]`, template strings
* `gold::lang` – TypeScript-like scripting whose runtime is gold data
* Pluggable backends: window, input, audio, render, dataStore
* Web module: Express-like HTTP server, HTML/CSS parsers, file-based dataStore
* Game module: component-based engine, bgfx rendering, SDL3 window/input
* UI module: HTML/CSS renderer (cascade, block/flex/grid layout, CPU rasterizer) driving a texture in-world

## Quick start
```bash
git submodule update --init --recursive
# The UI module's CPU rasterizer is always on. The optional Ultralight (WebKit)
# backend needs the SDK in 3rdParty/ultralight-free-sdk; enable it with
# -DGOLD_UI_ULTRALIGHT=ON (off by default).
cmake -S . -B build -DGOLD_BUILD_GAME=OFF -DGOLD_BUILD_WEB=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

## Examples (build with `-DGOLD_BUILD_EXAMPLES=ON`)
* `examples/langExample` – the interpreter, the `script` facade, and sharing
  gold data between host and script
* `examples/blahajExample` – loading and inspecting a glTF asset
* `examples/conwaysGameOfLife` – a small entity-driven simulation on the game
  engine (requires `GOLD_BUILD_GAME`)
* `examples/myWebProject` – a full-stack web service: routes, HTML builders,
  sessions, and the file-backed data store (requires `GOLD_BUILD_WEB`)

## Docs
- [Core Types](core/types.md)
- [var / object / list](core/var_object_list.md)
- [goldjs sugar](core/goldjs.md)
- [Serialization](core/serialization.md)
- [gold::lang scripting](lang/scripting.md)
- [Web module](web/overview.md)
- [UI module](ui/overview.md)
- [Game module](game/overview.md)
- [Backends](backends/overview.md)
- [Build & config](build.md)

## Conventions
See `AGENTS.md` for JS-style ergonomics, backend registration pattern, and gold-data-native events.

<!-- 
Agent note: This index is the entry point for developers and AI agents. Keep links in sync with files created by subagents. Update when new modules are added.
-->
