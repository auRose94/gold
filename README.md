# [gold](https://github.com/auRose94/gold) - Generic Object Linked Development 
### A high level app framework

[![License](https://img.shields.io/badge/license-Apache%202-blue)](https://github.com/auRose94/gold/LICENSE)

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
* Express.JS "like" HTTP(S)/WebSocket server (libwebsockets transport as a
  loadable plugin)
* HTML5 rendering (with form handling/pragmatic templating)
* Embedded document store ("file" backend: JSON files on disk; pluggable
  via the dataStore interface) and basic MVC system 
* Server-side image loading (not connected to game).
* Object & component based game engine: entities with transform
  hierarchies, cameras, punctual lights, environment maps, glTF mesh
  import rendered through a PBR shader (or flat/unlit), sprites, physics
  (bullet as a plugin), and a headless-friendly frame loop
* HTML/CSS UI renderer: cascade, block/flex/grid layout, anti-aliased CPU
  rasterizer, text through the system FreeType (dlopen'd, with a built-in
  fallback font) — re-renders only when the UI changes, and drives
  in-world surfaces (`uiSurface`: a CRT on a desk) with pointer
  picking and click/hover events
* Pluggable everything: window, input, audio, render and physics backends
  live behind pure interfaces, registered by name, selected from your
  config or **the command line** (`--render-backend=sdlgpu`), with
  fallback chains; optional backends are dynamically-loaded plugins
* An integrated MCP (Model Context Protocol) debug server
  (`--mcp=8090`, config key `"mcp"`): an agent over localhost HTTP can
  read/write the running app's live gold data, call methods, eval
  gold::lang, query/mutate the UI, and pull rendered screenshots
  (`libgoldSdl3.so`, `libgoldBgfx.so`, ...) — nothing platform-specific
  is baked in
* Auto shader compilation, with inlining — through the backend that owns
  it; the compiler tool never links into the framework
* CMake utilities
* Still experimental threading stuff (workers/promises)
* Hard parts of C++ have been abstracted to JS/Python difficulty
* Works with GCC and Clang on C++26 (MSVC is untested).
* Uses little memory actually, good enough for x64 IOT or Mobile.

Where it falls short?
* Lacks some in-depth error handling (see genericError)
* Threading is subsystem limited and experimental (off by default)
* Has heavy 3rdParty dependencies that have it's own dependencies (but
  they're all system packages behind loadable plugins — see the
  dependency policy below)
* Needs the latest bleeding edge compiler and STL library

What's lacking?
* Documentation is growing (`docs/index.md` and the module tree) but far
  from complete
* Comments
* The SDL_GPU render backend is the new kid: the resource and draw path
  work (the shark example renders through it) but it's still catching up
  to bgfx feature-wise

What's planned?
High priority:
* Complete the SDL_GPU render backend's parity with bgfx
* Expanded game engine stability and rendering coverage
* Expanded web services and persistence guarantees

Medium priority:
* Audio handling and resource lifecycle support
* Controller handling
* Async event handling
* In-depth complex examples
* Documentation

Lower priority:
* Game editor
* Compile to WebAssembly/ASM.JS?

# Getting Started

You can copy everything from the examples directory to get started with a basic web app or game. It's better to make this project a submodule in git instead of cloning/copying the project.

Build (or grab the short way, `./build.sh`) and run the shark:

```sh
./build.sh                      # core + game + web + ui + lang, tests, examples
ctest --test-dir build --output-on-failure

./build/examples/blahajExample/BlahajExample                      # default: bgfx
./build/examples/blahajExample/BlahajExample --render-backend=sdlgpu
./build/examples/blahajExample/BlahajExample --window-backend=sdl2
./build/examples/blahajExample/BlahajExample \
  --window-backend=wayland,sdl --renderer=Vulkan path/to/model.gltf
```

All code not in 3rdParty or explicitly stated otherwise are Apache version 2.

## Building

Everything (modules, tests, examples + the test run) is one script:

```sh
./build.sh            # build everything, run the suite
./build.sh --core     # shared core only (fast, smallest dependency surface)
./build.sh --clean    # wipe and configure from scratch
./build.sh --no-test  # configure and build, skip the test run
```

Under it the project uses CMake (3.16+) and builds with **C++26**
(GCC ≥ 13 / Clang ≥ 16; `CMAKE_CXX_STANDARD 26`). Submodules must be
present:

```sh
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Build options (all default to `ON` except examples):

* `GOLD_BUILD_GAME` – the game engine module (`gold::game`)
* `GOLD_BUILD_WEB` – the web server module (`gold::web`)
* `GOLD_BUILD_UI` – the HTML/CSS UI renderer module (`gold::ui`, requires `GOLD_BUILD_WEB`)
* `GOLD_BUILD_LANG` – the scripting-language module (`gold::lang`)
* `GOLD_BUILD_TESTS` – the test suite
* `GOLD_BUILD_EXAMPLES` – build the example projects (default `OFF`; requires `GOLD_BUILD_GAME`)
* **bgfx** comes from the system package (`libbgfx.so` + headers + the
  `bgfx-shaderc` tool, e.g. the `bgfx-cmake` package on Arch) — the game
  module cannot be configured without it. Shaders compile at build time
  and at run time through the external tool; the compiler is never
  statically linked into `gold::game`.

## Backends

Every pluggable subsystem follows the same pattern: a pure virtual
interface with **no platform types in its headers**, a `registerX(name,
factory)` registry, and selection by name — from the engine's settings
file (`~/.local/share/<company>/<game>/config.json`) or straight from
the command line. A name accepts a fallback chain (a list tried in
order), and backends an app hasn't built resolve through dynamically
loaded plugins that self-register on load.

| Subsystem | Facade | Backends |
| --- | --- | --- |
| Window | `window` | `sdl`/`sdl3`, `sdl2`, `wayland`, `headless` |
| Input | `inputSystem` | `sdl`/`sdl3`, `sdl2`, `evdev` |
| Audio | `audioSystem` | `sdl`/`sdl3`, `sdl2` |
| Render | `gfxBackend` | `bgfx` (default), `sdlgpu` |
| Physics | `world` | `bullet`, `none` |
| Data store | `database` | `file` (built-in) |

Selection precedence: **defaults < config.json < command line**.

```sh
./myApp --window-backend=NAME[,FALLBACK...]   # window/input system pick
./myApp --render-backend=NAME                 # render backend pick
./myApp --renderer=NAME                       # the renderer-API hint
```

`--window-backend` understands the comma fallback chain. `engine`
constructs with `argc`/`argv` and applies the flags over the settings
file; unknown flags and positional arguments pass through untouched.

The window config always appends `headless` as the last resort, so apps
never hard-fail on a missing compositor:

```json
{ "backend": ["wayland", "sdl", "headless"] }
```

Plugins follow the `libgold<Name>.so` naming convention for the backend
they provide — `libgoldSdl3.so` (window/input/audio + the `sdlgpu`
renderer), `libgoldSdl2.so` (SDL2 parity), `libgoldBgfx.so` (the bgfx
renderer), `libgoldBullet.so` (physics), `libgoldLws.so` (the web
transport). The loader searches `GOLD_PLUGIN_PATH`, then the
executable's directory, then the module directory; an unknown render
backend name falls back to bgfx rather than failing the app.

Native window/input backends: `"sdl"`/`"sdl3"` registers both names and
ships Wayland and X11 drivers (on Wayland gold creates the
`wl_egl_window` the render backend needs); `"wayland"` is a native
xdg-shell implementation; `"headless"` is built in. Input arrives as
gold objects — keyboard/mouse through the window event stream, and the
SDL input backend adds gamepad, touch and sensor events. The engine loop
is a plain pump + dispatch, no platform switch:

```cpp
gold::object ev;
while (ws->poll(ev))
    win.handleEvent({ev});
```

### Render backends

Rendering is abstracted behind `renderBackend` (gold-native types — no
bgfx or Vulkan types in headers; handles are opaque `uint16` indices).
The default implementation wraps bgfx; `sdlgpu` drives SDL_GPU
(Vulkan-class, SPIR-V) through the same interface — entity/component
resource tables, the view/model pipeline, PBR draws, readback
screenshots and frame pacing (the shark renders through it; parity work
continues). An unknown backend falls back to bgfx, so a missing plugin
degrades instead of breaking, and implementing the interface directly
against a driver is the migration path for whichever renderer you want.

The engine loop runs at a configurable frame rate (`"frameTime"` ms in
the game's `config.json`, default 16 → 60fps) so it does not peg the CPU
when the compositor does not present/vsync; the SDL backend picks
MAILBOX presentation so that cap cannot straddle a vsync deadline.

### Events & handlers are gold data

Backends emit window/input events as gold `object`s
(`{"type","resized","width",800,"height",600}`, `{"type","key_down",
"keyCode",...}`), so they are serializable, loggable, and dispatchable.
The `window` prototype exposes handler slots — `onQuit`, `onResized`,
`onKeyDown`, `onMouseWheel`, etc. — whose defaults update window state;
apps override them with `setFunc`/`setMethod`.

## Module layout

* `gold::shared` (`libgoldShared.a`) is the core: `var`/`list`/`object`,
  serialization, files, crypto, and the `gold::module` runtime loader.
* `gold::game` (`libgoldGame.so`) and `gold::web` (`libgoldWeb.so`) are
  shared libraries built on top of the core, as is `gold::ui`
  (`libgoldUI.so`), the HTML/CSS renderer the game module's `uiSurface`
  component draws in-world. Linking them is optional:
  * At **build time**, `GOLD_BUILD_GAME` / `GOLD_BUILD_WEB` / `GOLD_BUILD_UI`
    control whether they are built at all (`GOLD_BUILD_UI` needs the web
    module, for the HTML and CSS parsers).
  * At **run time**, `gold::module::load("game")` / `("web")` / `("ui")`
    dlopen's the module on demand, so an app that only needs the core never
    pulls in the game engine or web stack. Point `module::setLibraryPath()`
    at the directory containing the `.so` files when they aren't on the
    loader path.

Because the game/web modules are shared libraries, example executables link
dynamically instead of statically baking in the entire framework (the game
example dropped from ~240MB to under 1MB).

The test suite is a set of CTest targets over the whole stack —
`goldTests` (core), `goldFileErrorTests`, `goldSubsystemTests`,
`goldLangTests`, `goldGameTests`, `goldRenderBackendTests`,
`goldSDLGpuTests`, `goldWebTests`, `goldWebPersistenceTests`,
`goldServerTests`, `goldUITests` — run with
`ctest --test-dir build --output-on-failure`.

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

## Dependency policy

Runtime loads, not vendored source. Optional subsystems are shared
libraries (`libgoldGame.so`, ...) that apps link or pull in at run time
(`module::load`); platform backends are plugins compiled against their
system dependency's headers and loaded on demand (`include/plugin.hpp`,
`plugin::load`); C-ABI dependencies (FreeType) are dlopen'd directly by
soname with graceful fallbacks (the UI's built-in 5x7 font when FreeType
is absent). Selection policy: system package → platform-parity package →
feature absent.

Everything platform-shaped ships as a plugin over a **system package**:
SDL3 (`libgoldSdl3.so`: window/input/audio/render), SDL2
(`libgoldSdl2.so`: window/input/audio parity), bgfx (`libgoldBgfx.so`:
the renderer, plus the build-time shader compiler tool), bullet
(`libgoldBullet.so`: physics), libwebsockets (`libgoldLws.so`: the web
transport). Nothing platform-shaped is vendored anymore — the in-tree
sources are JSON/binary codec implementations (`src/goldjson.cpp`), the
web document store (`src/web/dataStoreFile.cpp`), and the generated
build input in `3rdParty/generated`. Crypto++/snappy, mongo-c-driver,
zlib, libuv, SDL/SDL_image/SDL_ttf and brtshaderc were removed entirely
(dead in the build graph, or replaced — OpenSSL provides PBKDF2/base64).

Install the system packages your chosen backends need (`sdl3`, `sdl2`,
`bgfx-cmake`/`libbgfx.so`, `bullet`, `libwebsockets`, `openssl`); the
core-only build (`./build.sh --core`) needs none of them.

## Security notes

* `object::generateHash` uses PBKDF2-HMAC-SHA256 with 600,000 iterations
  (OWASP-recommended) rather than the old 1,024. This is intentionally slow
  for password hashing; existing stored hashes are unaffected but should be
  re-derived on next login.