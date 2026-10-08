# Build & configuration

Gold builds with CMake (3.16+) and **C++26** (GCC ≥ 13 / Clang ≥ 16;
`CMAKE_CXX_STANDARD 26`). Submodules must be present.

## Quick start

```sh
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Build options

All default to `ON` except examples:

| Option | Default | Purpose |
| --- | --- | --- |
| `GOLD_BUILD_GAME` | `ON` | game engine module (`gold::game`) |
| `GOLD_BUILD_WEB` | `ON` | web server module (`gold::web`) |
| `GOLD_BUILD_UI` | `ON` | HTML/CSS UI renderer module (`gold::ui`, requires `GOLD_BUILD_WEB`) |
| `GOLD_BUILD_LANG` | `ON` | scripting-language module (`gold::lang`) |
| `GOLD_BUILD_TESTS` | `ON` | test suite |
| `GOLD_BUILD_EXAMPLES` | `OFF` | example projects (requires `GOLD_BUILD_GAME`) |

**bgfx is always the system package** (`libbgfx.so` + headers + the
`/usr/bin/bgfx-shaderc` tool, e.g. the `bgfx-cmake` package on Arch); the
vendored fallback build was removed. Without the package, the game module
cannot be configured.

## Fast core-only build (no game/web deps)

```sh
cmake -S . -B build -DGOLD_BUILD_GAME=OFF -DGOLD_BUILD_WEB=OFF
cmake --build build --target goldTests
ctest --test-dir build --output-on-failure
```

## The `build.sh` wrapper

`./build.sh` configures, builds, and runs the test suite:

- `./build.sh` — everything + examples, run tests
- `./build.sh --core` — shared core only (fast), run tests
- `./build.sh --clean` — wipe the build directory first
- `./build.sh --no-test` — configure and build, skip the test run

## System dependencies

- **OpenSSL** — required (PBKDF2 password hashing, URL-safe base64, and the
  web module). e.g. `libssl-dev` on Debian/Ubuntu.
- **SDL3** — optional: the game module's SDL backends (window/input/audio
  + SDL_GPU render) are the loadable `libgoldSdl3` plugin, built when the
  system `sdl3` pkg-config module is found. Without it the engine degrades
  to wayland/evdev/headless and bgfx's Noop renderer. e.g. `sdl3` on Arch,
  `libsdl3-dev` on Debian.
- **bgfx** — required for the game module (system package).

Other dependencies (bullet, freetype, uWebSockets/uSockets) still build from
`3rdParty/` sources for now; they migrate one by one to runtime dynamic
loading — backend code moves into optional plugin shared libraries that
require nothing but their system package at build time (`feature/dynamic-
backends`, phases 2+).

## Module layout

- `gold::shared` (`libgoldShared.a`) — the core: `var`/`list`/`object`,
  serialization, files, crypto, and the `gold::module` runtime loader.
- `gold::game` (`libgoldGame.so`) and `gold::web` (`libgoldWeb.so`) — shared
  libraries built on top of the core. Loaded on demand at run time via
  `gold::module::load("game")` / `("web")`; point `module::setLibraryPath()`
  at the directory containing the `.so` files when they aren't on the loader
  path.
