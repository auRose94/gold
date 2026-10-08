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