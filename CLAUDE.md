# Agent quickstart

This file is the entry point for AI agents working in this repo. Read it
first, then follow the links.

## Start here

- **`AGENTS.md`** — project conventions, repository map, test suite, and the
  "verify your change" checklist. Read this before writing any code.
- **`docs/index.md`** — documentation index (module docs, build & config).
- **`README.md`** — overview, feature list, and build instructions.

## What this project is

Gold is a C++20/26 data-driven framework with JS/TS-style ergonomics. The
core is `var` (a universal dynamic value) plus `object`/`list` containers,
with `goldjs.hpp` sugar (`jo`/`ja`, `v["key"]`, template strings). On top of
the core sit three optional modules: `gold::lang` (a TypeScript-like
interpreter), `gold::game` (component engine + bgfx/SDL3), and `gold::web`
(Express-like HTTP/WebSocket server + file dataStore).

## Golden rules

1. **Follow the JS-style ergonomics** in `AGENTS.md` — use `jo`/`ja`,
   `v["key"]`, and `tpl(...)`, not the verbose accessors.
2. **Never edit `3rdParty/`** — it's git submodules.
3. **Ship tests with new behavior** — add a `TEST(...)` block to the matching
   file in `tests/` (see the test table in `AGENTS.md`).
4. **Verify before finishing** — run `./build.sh --core` (always) and the full
   `./build.sh` if you touched `lang/`/`game/`/`web/`.

## Common commands

```sh
./build.sh --core          # fast core build + tests
./build.sh                 # full build (modules + tests + examples) + tests
ctest --test-dir build --output-on-failure   # run all tests
cmake --build build --target goldTests && ./build/tests/goldTests  # one target
```

## Where things live

- Public headers: `include/` (core) and `include/{game,lang,web}/`.
- Implementation: `src/` (core) and `src/{game,lang,web}/`.
- Build recipes: `cmake/`.
- Tests: `tests/` (see the target table in `AGENTS.md`).
- Docs: `docs/` (`docs/index.md` is the entry point).
