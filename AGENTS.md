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

<!-- lean-ctx -->
## lean-ctx

lean-ctx is active — the MCP tools replace native equivalents.
Full rules: LEAN-CTX.md (open on demand — do not auto-load).
<!-- /lean-ctx -->
