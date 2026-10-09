# Plugins: runtime-loaded backends

gold does not bake optional backends into heavy binaries. Every
subsystem's `createX(name)` registry is the seam: a backend can be
statically registered (built into its module) or provided by a **plugin**
— a shared library that is found and loaded at run time.

## Gold backend plugins

A gold plugin is this repo's own backend code, built as a shared library
against the system package's *headers* it wraps (SDL3, bgfx, ...). The
library itself is opened at run time. Naming convention:

```
backend name "sdl3"      ->  libgoldSdl3.so / .dylib / .dll
backend name "fixtureWindow"  ->  libgoldFixtureWindow.so
```

Generic config aliases expand through `plugin::pluginCandidates`:
`"sdl"` probes `sdl3` first, then the SDL2 parity adapter (`libgoldSdl2` —
window/input/audio only; SDL2 has no SDL_GPU, so rendering falls back to
bgfx); `"sdlgpu"` probes `sdl3`. A plugin that provides an alias
registers both its concrete name and the alias, and the first candidate
found wins.

**Physics** follows the same seam: `createPhysicsBackend` (a
`physicsBackend` interface in `include/game/physicsBackend.hpp`)
resolves `world(jo("physics", "bullet"))` — the bullet backend ships as
`libgoldBullet`, compiled against the system bullet package (use the
double-precision `bullet-dp` build; gold mirrors `BT_USE_DOUBLE_PRECISION`
so the ABIs agree). Without it the world facade runs the built-in no-op
`"none"` backend: worlds initialize, bodies register nowhere, transforms
stay under script control. Shape components are descriptor data
("shapeKind" + size/mesh/node); the backend materializes collision
shapes at body time.

**The web server's transport** is a plugin seam too
(`createServerTransport`): `server.start()` resolves a transport by the
config name, which consumes the buffered routes and blocks on its own
loop. `libgoldLws` (backend `"lws"`) is the real engine: HTTP over the
system libwebsockets package (headers only at build; sonames
"websockets" probe `libwebsockets.so.21` first — 4.x context structs
change between minors), routes with `:param` patterns matched in gold,
static-mount fallbacks shared with the facade, WebSocket routes
(`server.ws(pattern, {open, message, close})`), optional
`"sslCert"`/`"sslKey"` HTTPS, and a graceful `server.stop()` (the loop
returns, start() unblocks). Without the system package the build skips
the plugin and `start()` reports "no server transport available".

Loading one is all that is needed: the plugin's static initializers call
the standard registrars (`registerWindowSystem`,
`registerInputSystem`, `registerAudioSystem`,
`registerRenderBackendName`, `registerDataStore`,
`registerRenderer`), so the very next `createX(name)` (or chain) finds
the backend. The registries implement this automatically: a name that
misses triggers `plugin::load(name)` and then retries — see
`src/game/windowSystem.cpp` for the reference implementation.

Plugins stay loaded for the process lifetime (their registrar-written
factory pointers would dangle after `dlclose`); `plugin::isLoaded()`
reports what happened. Failed loads are explained by
`plugin::lastError()`.

## System C libraries

Dependencies with a stable C ABI are not even compiled into gold. A thin
shim (`src/ui/ftShim.*` is the reference) dlsyms a dozen entry points
from a dlopen'd system library:

```cpp
bool ok = ftshim::open();               // plugin::openSystemLibrary + typed dlsyms
if (!ok) ... // built-in font fallback
```

The dependency policy — encoded in `plugin::sonames(name)` and probed via
`plugin::probe(name)`:

1. **System package** for this platform,
2. a **platform parity package** (an equivalent library this platform
   ships instead), then
3. **feature absent** — engines degrade to their fallback backends
   (headless window, built-in font, software renderer).

On platforms without system package managers (e.g. Windows), the loader's
executable-directory search path doubles as the "bundled" location:
ship the plugin next to the application.

## Testing the seam

The test suite builds `libgoldFixtureWindow` (a real shared library that
self-registers a window backend) into the executables' directory and
`tests/gameTest.cpp` exercises the miss → load → registered → fallback
chain end to end; `tests/subsystem_test.cpp` covers the loader API
itself.