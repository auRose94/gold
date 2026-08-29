# [gold](https://github.com/CoryNull/gold) - Generic Object Linked Development 
### A high level app framework

[![License](https://img.shields.io/badge/license-Apache%202-blue)](https://github.com/CoryNull/gold/LICENSE)

What is this?

It's a high level abstraction app framework, written in C++. It uses simple objects to carry out various tasks in a simple data driven development. Making fast video games or HTTP web services can be organically and quickly developed, and both sub systems are split accordingly, and they'll both share the same base. You can make a game server that uses the same code base as the game. Or a website that talks to a game, or vice-versa. More additions and ideas coming down the road.

What's in the box?
* RAII Objects/Lists
* Object inheritance (prototypes)
* Type agnosticism with abstracted run-time type information
* Universal generic RAII value container; "var" <-- type name
* First class Vector, Quaternion, Matrix3x3, Matrix4x4 var types
* JSON/BSON/CBOR/MsgPack/UBJSON/URLForm serialization
* JavaScript-style ergonomics (`v["key"]` read/write, `jo`/`ja` literals,
  template strings, array helpers — `goldjs.hpp`)
* A TypeScript-like scripting language (`gold::lang`): optional type
  annotations, classes, arrows, template literals — values and scopes are
  gold objects, and scripts share gold data/backends with the host
* Express.JS "like" HTTP(S)/WebSocket server
* HTML5 rendering (with form handling/pragmatic templating)
* Embedded document store ("file" backend: JSON files on disk; pluggable
  via the dataStore interface) and basic MVC system 
* Server-side image loading (not connected to game).
* Object & component based game engine
* Basic window handling
* 3D matrix transformation hierarchies
* Texture loading(2D/3D/Cube)
* Auto shader compilation, with inlining
* CMake utilities
* Still experimental threading stuff (workers/promises)
* 3rdParty dependencies are sub modules to other GitHub projects
* Hard parts of C++ have been abstracted to JS/Python difficulty
* Works with GCC and Clang (MSVC is untested).
* Uses little memory actually, good enough for x64 IOT or Mobile.

Where it falls short?
* Lacks some in-depth error handling (see genericError)
* Threading is subsystem limited and experimental (off by default)
* Has heavy 3rdParty dependencies that have it's own dependencies
* Needs the latest bleeding edge compiler and STL library
* Uses the C++17/20 standard

What it's lacking?
* Documentation
* Comments
* Bug/error testing
* Tests
* A website

What's planned?
High priority:
* Error reporting and consistent failure semantics
* Complete asset handling and the glTF pipeline
* Expanded game engine stability and rendering coverage
* Expanded web services and persistence guarantees

Medium priority:
* Audio handling and resource lifecycle support
* Controller handling
* Async event handling
* GUI handling
* In-depth complex examples
* Documentation

Lower priority:
* Game editor
* Compile to WebAssembly/ASM.JS?

# Getting Started

You can copy everything from the examples directory to get started with a basic web app or game. It's better to make this project a submodule in git instead of cloning/copying the project.

All code not in 3rdParty or explicitly stated otherwise are Apache version 2.

## Building

The project uses CMake (3.16+) and builds with **C++26** (GCC ≥ 13 / Clang ≥ 16;
`CMAKE_CXX_STANDARD 26`). Submodules must be present:

```sh
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Build options (all default to `ON` except examples):

* `GOLD_BUILD_GAME` – the game engine module (`gold::game`)
* `GOLD_BUILD_WEB` – the web server module (`gold::web`)
* `GOLD_BUILD_TESTS` – the test suite
* `GOLD_BUILD_EXAMPLES` – build the example projects (default `OFF`; requires `GOLD_BUILD_GAME`)
* `GOLD_USE_SYSTEM_BGFX` – use an installed bgfx (`libbgfx.so` + the
  `bgfx-shaderc` tool) instead of building the bundled bgfx/bx/bimg
  submodules and the bundled shader-compiler stack (glslang, spirv-tools,
  glsl-optimizer, fcpp). Defaults to `ON` when a system bgfx is found;
  falls back to the bundled sources otherwise. Using the system install
  cuts a full build to tens of seconds. Requires the `bgfx` CMake package
  and `/usr/bin/bgfx-shaderc` (e.g. the `bgfx-cmake` package on Arch).

With system bgfx, shaders are compiled at build time and at runtime by
the external `bgfx-shaderc` tool; the runtime shader compiler is not
statically linked into `gold::game`.

To build and test just the shared core (fast, no game/web deps):

```sh
cmake -S . -B build -DGOLD_BUILD_GAME=OFF -DGOLD_BUILD_WEB=OFF
cmake --build build --target goldTests
ctest --test-dir build --output-on-failure
```

With the game module enabled the suite also builds `goldGameTests` for
window/input backend and gold-event dispatch coverage; `ctest` runs both.

To build the example projects in-tree:

```sh
cmake -S . -B build -DGOLD_BUILD_EXAMPLES=ON
cmake --build build --target ConwaysGameOfLife MyWebProject
```

> Crypto (PBKDF2 password hashing, URL-safe base64) is provided by the system
> OpenSSL library, which is also required by the web module. The bundled
> Crypto++ submodule has been removed. Install it with your system package
> manager if missing (e.g. `libssl-dev` on Debian/Ubuntu).
>
> The game module uses system SDL3 (`libSDL3`, `sdl3` pkg-config module) for
> its `"sdl"` window backend; the bundled SDL2 submodule has been removed.
> Install it if missing (e.g. `sdl3` on Arch, `libsdl3-dev` on Debian).

## Security notes

* `object::generateHash` uses PBKDF2-HMAC-SHA256 with 600,000 iterations
  (OWASP-recommended) rather than the old 1,024. This is intentionally slow
  for password hashing; existing stored hashes are unaffected but should be
  re-derived on next login.

## Module layout

* `gold::shared` (`libgoldShared.a`) is the core: `var`/`list`/`object`,
  serialization, files, crypto, and the `gold::module` runtime loader.
* `gold::game` (`libgoldGame.so`) and `gold::web` (`libgoldWeb.so`) are
  shared libraries built on top of the core. Linking them is optional:
  * At **build time**, `GOLD_BUILD_GAME` / `GOLD_BUILD_WEB` control whether
    they are built at all.
  * At **run time**, `gold::module::load("game")` / `("web")` dlopen's the
    module on demand, so an app that only needs the core never pulls in the
    game engine or web stack. Point `module::setLibraryPath()` at the
    directory containing the `.so` files when they aren't on the loader path.

Because the game/web modules are shared libraries, example executables link
dynamically instead of statically baking in the entire framework (the game
example dropped from ~240MB to under 1MB).

### Scripting language (`gold::lang`)

A TypeScript-like interpreter whose runtime IS gold: values are `var`/`object`/
`list`, the AST is built from gold objects, and scopes reuse gold's prototype
chain. Type annotations are optional and can be enforced at runtime.

```cpp
auto r = gold::langRun(
  "class Point { constructor(x, y) { this.x = x; this.y = y; } "
  "  len() { return this.x * this.x + this.y * this.y; } } "
  "const p = new Point(3, 4); p.len();", gold::object(), false);
// r == 25
```

The `script` object embeds a script in a host app: `setGlobal`/`getGlobal`
share gold objects with the host, `eval` evaluates expressions, and `call`
invokes functions — so scripts can use gold backends directly.

### Data store

The web module's persistence is a `dataStore` backend behind the
`database`/`collection`/`model` facade (config `"backend"`):
* `"file"` — the default: each database is a directory, each collection a
  subdirectory, and each document a JSON file named by `"_id"`. Filters
  support equality and `{"$in", [...]}`; writes are atomic (temp+rename).
  Point it at a directory with `{"path", "./data"}`.

This replaces the MongoDB driver entirely (no server, no driver), keeping
the same document-oriented API.

### Window system backends

Window creation is abstracted behind `windowSystem` (a pure interface with no
SDL/bgfx types in its headers). The `window` object is a facade over a
backend chosen by name:

* `"sdl"` / `"sdl3"` — SDL3 window + input events (registered under both
  names). SDL3 ships Wayland and X11 drivers (native handles come from SDL
  window properties; on Wayland gold creates the `wl_egl_window` the EGL
  render backend needs).
* `"wayland"` — native Wayland (xdg-shell) window + wl_seat input.
* `"headless"` — no real window; for tests, CI, and offscreen rendering.

Backends are registered via `registerWindowSystem()`. The `"backend"` config
accepts a string or a list of names tried in order (fallback chain);
`"headless"` is always appended last so apps never hard-fail on a missing
compositor:

```json
{ "backend": ["wayland", "sdl", "headless"] }
```

### Input backends

`inputSystem` abstracts device capture. Backends:

* `"evdev"` — real device capture via libevdev (`/dev/input/event*`).
* `"sdl"` — SDL3's unified input: gamepad (`gamepad_button`/`gamepad_axis`/
  `gamepad_touchpad` events), touch (`touch_down`/`touch_move`/...), and
  sensors (`sensor`). Keyboard/mouse continue to arrive through the window
  event stream; the SDL input backend re-pushes those events so the window
  backend still sees them.

### Audio backends

`audioSystem` abstracts playback. Backends:

* `"sdl"` — SDL3 audio: opens the default device, loads WAVs, and plays
  them through SDL3 audio streams (overlapping playback, per-stream volume).

### Render backends

The render backend binds to the platform window through `windowSystem::native()`.
Select it with the graphics config `"renderBackend"` (`"bgfx"` default,
`"sdlgpu"` for the SDL3 GPU backend, `"vulkan"` reserved); it falls back to
bgfx when unavailable. The SDL3 GPU backend currently drives the window's
swapchain (device init, clear, present); the full resource/draw pipeline is
in progress.

The engine loop runs at a configurable frame rate (`"frameTime"` ms in the
game's `config.json`, default 16 → 60fps) so it does not peg the CPU when
the compositor does not present/vsync.

### JavaScript-style ergonomics

`var` is a universal dynamic value, and `include/goldjs.hpp` adds JS/TS-flavored
sugar so gold code reads like a scripting language:

```cpp
using namespace gold;

auto user = jo("name", "bob", "age", 30, "score", 9.5, "tags", ja("admin", "dev"));
user["age"] = 31;                    // property write (writes through)
user["meta"]["active"] = false;      // nested access
user["tags"][0] = "staff";           // list element write

string name = user["name"];          // implicit conversions for reads
int64_t age  = user["age"];
bool   admin = user["admin"];
if (user["meta"]["active"]) { /* ... */ }

auto msg = tpl("Hello $0, you are $1", user["name"], user["age"]);  // template strings

auto evens = filter(ja(1,2,3,4,5), func([](list a){ return var(a[0].getInt64() % 2 == 0); }));
auto js  = toJSON(user);             // JSON.stringify
auto obj = fromJSON(js);             // JSON.parse
```

Property access returns a `varRef` proxy (read + write), and the `jo`/`ja`
object/array builders, `tpl` template strings, and `each`/`mapArr`/`filter`/
`findArr`/`join` array helpers remove most boilerplate.

### Events & handlers are gold data

Backends emit window/input events as gold `object`s (`{"type","resized",
"width",800,"height",600}`, `{"type","key_down","keyCode",...}`), so they are
serializable, loggable, and dispatchable. The `window` prototype exposes
handler slots — `onQuit`, `onResized`, `onMoved`, `onKeyDown`, `onMouseMove`,
etc. — whose defaults update window state. Apps override them with
`setFunc`/`setMethod`:

```cpp
win.setFunc("onResized", func([&](gold::list args) -> gold::var {
    auto ev = args[0].getObject();   // or args[1] for self+event
    return ev.getInt32("width");
}));
```

The engine loop is then a plain pump+dispatch with no platform switch:

```cpp
gold::object ev;
while (ws->poll(ev))
    win.handleEvent({ev});
```

### Render backends

Rendering is abstracted behind `renderBackend` (gold-native types — no bgfx
or Vulkan types in headers). `gfxBackend` creates the backend through
`createRenderBackend(renderBackendType)`:

* `BGFX` (default) — the current implementation, wrapping bgfx.
* `Vulkan` / `OpenGL` — reserved for future direct implementations.

Handles are opaque `uint16` indices owned by the backend, so a Vulkan backend
can replace bgfx without changing the engine's resource classes. This is the
migration path to deprecate bgfx: implement the `renderBackend` interface
against Vulkan and swap it in.

## Submodule policy

The `3rdParty` submodules track upstream branches. `zlib` and `libuv` are
kept at their latest releases, and `bullet3` and `freetype2`
are updated to their latest master commits. JSON and
the binary data formats (BSON/CBOR/MsgPack/UBJSON) are implemented in-tree
(`src/goldjson.cpp`) — the nlohmann/json submodule was removed. The web
module's document store is implemented in-tree too (`dataStore` interface
with a "file" backend in `src/web/dataStoreFile.cpp`), so the
mongo-c-driver is no longer built. The
`bgfx`/`bx`/`bimg`/`brtshaderc` sources are only built as a fallback when no
system bgfx is installed (`GOLD_USE_SYSTEM_BGFX`). `uSockets`/`uWebSockets`
are pinned to a version matching the web module's usage (their latest
releases changed the app-construction API). SDL2 and SDL_image/SDL_ttf were
removed entirely: the game module uses system SDL3, and Crypto++/snappy were
removed earlier (OpenSSL provides PBKDF2/base64).
