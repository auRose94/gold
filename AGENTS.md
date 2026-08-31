# Project conventions

Gold is a C++20/26 framework whose guiding philosophy is **data-driven and
scripting-like ergonomics**: `var` is a universal dynamic value and the API
should read like JavaScript/TypeScript. New code must follow these
conventions.

## JS-style ergonomics (the default way to write gold code)

- **Build objects/arrays with `jo(...)` / `ja(...)`** (`include/goldjs.hpp`)
  instead of `obj({{"k", v}, ...})` / `list({...})`:

  ```cpp
  auto user = jo("name", "bob", "age", 30, "tags", ja("admin", "dev"));
  ```

- **Read/write through `varRef` (`v["key"]`, `v[i]`, `obj["key"]`,
  `li[i]`)** instead of `setString/getString/...` accessors:

  ```cpp
  user["age"] = 31;            // was: user.setInt64("age", 31)
  string n = user["name"];     // was: auto n = user.getString("name")
  ```

- **Template strings** with `tpl("...$0...$1", a, b)` instead of manual
  concatenation.

- **Use `var` + `auto`** for locals; let `var` hold strings, numbers,
  objects, lists interchangeably.

## When NOT to use the sugar

- `getX("key", default)` with a fallback stays as-is (`varRef` has no
  default). Reading a field whose stored type may differ from the accessor
  also stays explicit (`object::getString` returns the default for a
  non-string; `var::getString()` converts).
- Callbacks and method bindings keep `func`/`method` + `list args`.
- Internals that need exact types (binary codecs, physics) stay typed.

## Backends & configuration

Every pluggable subsystem uses the same registered-backend pattern:

- A pure virtual interface (`windowSystem`, `inputSystem`, `audioSystem`,
  `dataStore`, `renderBackend`).
- `registerX(name, factory)` + `createX(name)` registries.
- Selection via a config string (e.g. `{"backend", "file"}`), with a
  fallback chain when appropriate.

## Gold-data-native

Events, handler slots, and results cross subsystem boundaries as gold
`object`/`var` data, not platform structs. The engine loop is a plain
pump + dispatch with no platform switch.

## The scripting language (`gold::lang`)

`gold::lang` is a TypeScript-like interpreter whose runtime IS gold: values
are `var`/`object`/`list`, the AST is built from gold objects, and scopes use
gold's prototype chain. Type annotations are optional (parsed and stored; a
config flag enables runtime enforcement). Scripts share gold data with the
host via the `script` facade (`setGlobal`/`getGlobal`/`eval`/`call`).

## Build & verify

- Core: `./build.sh --core` then `ctest --test-dir build --output-on-failure`.
- Full: `./build.sh` (modules, tests, examples).
- New behavior ships with tests in `tests/test.cpp` / `tests/gameTest.cpp` /
  `tests/langTest.cpp`.

## Repository map

- `include/` — public headers. `goldjs.hpp` (JS sugar), `types.hpp` (`var`),
  `module.hpp` (runtime loader), `file.hpp`, `image.hpp`, `promise.hpp`,
  `worker.hpp`. Subdirs: `game/`, `lang/`, `web/`.
- `src/` — core implementation (`var.cpp`, `object.cpp`, `list.cpp`,
  `types.cpp`, `goldjson.cpp`, `file.cpp`, `module.cpp`, `image.cpp`,
  `promise.cpp`, `worker.cpp`). Subdirs: `game/`, `lang/`, `web/`.
- `cmake/` — build recipes: `shared.cmake`, `lang.cmake`, `game.cmake`,
  `web.cmake`, `tests.cmake`, `shaders.cmake`, `shadercParse.cmake`.
- `tests/` — test suite (see below).
- `examples/` — runnable projects (`langExample`, `blahajExample`,
  `conwaysGameOfLife`, `myWebProject`).
- `docs/` — module docs; `docs/index.md` is the entry point.
- `3rdParty/` — git submodules (SDL, bgfx, bullet3, etc.). Do not edit.

## Test suite

Tests use the `goldtest` micro-framework (`tests/goldtest.hpp`): a `TEST(name)`
macro registers a group, and `EXPECT` / `EXPECT_EQ` / `EXPECT_NE` /
`EXPECT_NEAR` / `EXPECT_TRUE` / `EXPECT_FALSE` record checks. Each test
executable is a separate CTest target:

| Target | File(s) | Module |
| --- | --- | --- |
| `goldTests` | `tests/test.cpp`, `tests/test_file_decode.cpp` | core |
| `goldFileErrorTests` | `tests/test_file_errors.cpp` | core |
| `goldSubsystemTests` | `tests/subsystem_test.cpp` | core |
| `goldLangTests` | `tests/langTest.cpp` | lang |
| `goldGameTests` | `tests/gameTest.cpp`, `tests/gameMeshAssetTest.cpp` | game |
| `goldRenderBackendTests` | `tests/renderBackendTest.cpp` | game |
| `goldWebTests` | `tests/webTest.cpp` | web |
| `goldWebPersistenceTests` | `tests/webPersistenceTest.cpp` | web |

To add a test: put a `TEST(...)` block in the matching file (or add a new
`.cpp` and register it in `cmake/tests.cmake`). Run the whole suite with
`ctest --test-dir build --output-on-failure`, or a single target with
`cmake --build build --target goldTests && ./build/tests/goldTests`.

## Verify your change

Before finishing, run the relevant build + tests and confirm they pass:

1. `./build.sh --core` (fast core + tests) — always.
2. If you touched `lang/`, `game/`, or `web/` code, run the full
   `./build.sh` so those module tests build and pass.
3. If you changed CMake or added a test file, confirm the new target is
   registered in `cmake/tests.cmake` and appears in `ctest -N`.
4. Keep the docs in sync: update `docs/index.md` and the relevant module doc
   when you add or rename a public API or module.

<!-- lean-ctx -->
## lean-ctx

lean-ctx is active — the MCP tools replace native equivalents.
Full rules: LEAN-CTX.md (open on demand — do not auto-load).
<!-- /lean-ctx -->
