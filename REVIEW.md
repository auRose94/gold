# Gold Framework — Code Quality Review

## P0: Build-Breaking Bugs (won't compile)

### `src/file.cpp:181-209` — Missing parameter lists

Six method implementations are missing their parameter lists entirely:

```cpp
// Current (broken):
var file::extension) {
var file::asJSON) {
var file::asBSON) {
var file::asCBOR) {
var file::asMsgPack) {
var file::asUBJSON) {

// Should be:
var file::extension(list args = {}) {
var file::asJSON(list args = {}) {
// ... etc
```

### `src/object.cpp:19-24` + `src/var.cpp:13-18` — Duplicate `objData` definition (ODR violation)

Both files define `struct objData` locally with the same fields. The header (`types.hpp:508`) forward-declares `struct objData` and uses `shared_ptr<objData>` in `object::ptr`. Having two independent definitions in different translation units is an ODR violation causing linker errors or undefined behavior.

### `src/object.cpp:672` + `src/list.cpp:127` — `use_count() <= 0` logic error

```cpp
// object.cpp:672
if (data && data.use_count() <= 0) { ... }  // BUG: use_count() >= 1 always

// list.cpp:127
if (data && data.use_count() <= 0) data->items.clear();  // BUG
```

`shared_ptr::use_count()` is always >= 1 for a non-null shared_ptr. The `<= 0` check is never true, so the destructor never clears items on the last reference. This causes memory leaks in objects/lists with many entries.

### `src/var.cpp:1077` + `src/var.cpp:1122` — Mat4x4 translation reads wrong index

```cpp
// var.cpp:1077 (Mat4x4Float + Vec3 -> translate)
getFloat(5),    // correct
getFloat(5),    // BUG: should be getFloat(6)
getFloat(6),    // correct
```

Same bug at line 1122 for Mat4x4Double. This corrupts the translation matrix, producing incorrect transforms.

---

## Architecture Assessment

### Strengths

1. **`var` type-erasure** with `shared_ptr<varContainer>` and inline scalar storage. Copy-on-write via shared_ptr is cheap for passing by value.

2. **`varRef` proxy** (`types.hpp:696-828`) enables `v["key"]` read/write through objects/lists — excellent JS-style ergonomics.

3. **`jo()`/`ja()` variadic templates** (`goldjs.hpp:33-58`) make object/array construction read like JS literals.

4. **`lang` interpreter uses gold as its runtime** — values are `var`/`object`/`list`, AST is gold objects, scopes use gold's prototype chain. Clever design.

5. **Backend registry pattern** is consistent across window/input/audio/render modules — `registerX(name, factory)` + `createX(name)`.

6. **5 serialization formats** (JSON, BSON, CBOR, MsgPack, UBJSON) in a single self-contained file (`goldjson.cpp`) with proper binary reader/writer infrastructure.

7. **PBKDF2 hashing** with 600k iterations (OWASP-compliant) in `object::generateHash`.

8. **Asset pack format** (`file::pack`/`unpack`) includes path traversal protection.

### Weaknesses

1. **60+ type enum** (`types.hpp:43-107`) is unwieldy. Vec2/3/4 x Int8/16/32/64/UInt8/16/32/64 x Float/Double = 48 vector types alone, plus quaternions and matrices.

2. **Massive code duplication** in `var.cpp`: each arithmetic operator (`+`, `-`, `*`, `/`, `%`, `<`, `>`, `<=`, `>=`) has a ~700-1200 line switch statement covering every type. `var.cpp` is 1654+ lines.

3. **`objData` defined in two TUs** (ODR violation) — see P0 above.

4. **`genericError` inherits from both `std::exception` and `object`** (`types.hpp:673`) — diamond-like ambiguity with virtual base issues.

5. **`operator==` compares by identity, not value** — `object::operator==` compares `id` fields, `var::operator==` compares `shared_ptr` addresses for objects/lists. Two equal objects are not `==`.

---

## Code Quality Issues (Gold's Own Code)

### Style / Readability

| Issue | Locations |
|---|---|
| `"gaurd"` typo in every lock declaration | `object.cpp:88,94,110,...` + many more in `list.cpp` |
| `using namespace std;` in header | `types.hpp:21-31` — pollutes the `gold` namespace |
| Manual `lock()`/`unlock()` instead of `lock_guard` | `promise.cpp:92-94,98-100`, `worker.cpp:40,60,97` |
| Commented-out lock | `object.cpp:598: // unique_lock<shared_mutex> gaurd(data->omutex);` |
| Global mutable object | `types.hpp:41: inline bx::DefaultAllocator defaultAllocator` |

### Threading / Concurrency

| Issue | Locations |
|---|---|
| `rend()` acquires lock but returns raw `reverse_iterator` — documented as unsafe but the lock is released before return | `list.cpp:304-308` |
| `nextJob()` in worker uses `try_lock` then `unlock` — another thread can steal the job between unlock and the caller's use | `worker.cpp:39-49` |
| `getObject()` has commented-out lock | `object.cpp:598` — read without synchronization |

### Logic Bugs

| Issue | Locations |
|---|---|
| Mat4x4 translation copies `getFloat(5)` twice | `var.cpp:1077,1122` |
| `operator==` for `object` compares by `id` not content | `object.cpp:665-669` |
| `var::operator==` for objects/lists compares `shared_ptr` addresses, not content | `var.cpp:263-264` |

---

## Security Concerns

| Concern | Severity | Location |
|---|---|---|
| Path traversal in `file::unpack()` | Low (protected) | `file.cpp:419` — checks for `..` and absolute paths |
| Path traversal in `file::pack()` | Low (protected) | `file.cpp:362-364` — rejects `..` and absolute paths |
| URL encoding: incomplete `%` escape | Low | `object.cpp:693-699` — `decodePercent` advances iterator but `isalnum` check may skip validation |
| Cookie parsing: incomplete `%` escape | Low | `object.cpp:745-751` — same pattern |
| Recursive directory read: symlink following | Medium | `file.cpp:253` — `follow_directory_symlink` option could allow symlink-based path escape |
| No input size limits on JSON/binary parsers | Low | `goldjson.cpp` — no depth or size limits on recursion |
| PBKDF2 on main thread | Low | `object.cpp:444-471` — 600k iterations blocks the calling thread |

---

## Test Coverage Assessment

### Good Coverage

- **Core types** (`test.cpp`): var construction, conversions, arithmetic, comparisons, vector types, object basics, list operations, JSON/BSON/CBOR/MsgPack/UBJSON roundtrips, file I/O, base64, asset pack, URL/cookie parsing, module loader, move semantics, JS sugar — ~40 TEST blocks, 635 lines
- **Language** (`langTest.cpp`): arithmetic, strings, objects, arrays, functions, control flow, classes, optional types, try/catch, iteration, builtins, REPL, script facade, logical operands, postfix update — ~15 TEST blocks, 184 lines
- **Game** (`gameTest.cpp`): window backends, gold events, handler override, input, image, setters, glTF loading (external buffers, normalized accessors, interleaved attributes, asset cache), GLB parsing, GPU texture cleanup, headless graphics, engine parallel updates — ~15 TEST blocks, 358 lines

### Missing / Weak Coverage

| Area | Status |
|---|---|
| **Worker system** (`worker.cpp`) | No dedicated tests |
| **Promise async threading** | Only `promise_synchronous_execution` test (single-threaded). No test of `useAllCores()` + `joinThreads()` path |
| **Web module** (`web/`) | Coverage unknown |
| **Expression strings** (`"child[0].name"`) | Only `object_expression_nested` — no deep nesting, no array index in middle of path |
| **Concurrent var/list/object** | No stress tests for multi-threaded access |
| **Error handling paths** | Only basic error checks. No tests for malformed input edge cases in all 5 binary formats |
| **Vector/setter edge cases** | Missing tests for `setInt64` through `setUInt8` on vec/mat types, `setExpression` with complex paths |
| **Prototype inheritance chain** | Only single-level test. No multi-level (grandparent) inheritance tests |
| **`varRef` proxy** | Covered in `js_sugar` test but not exhaustively (no nested writes, no type coercion edge cases) |

---

## Build System

### `build.sh`
- Clean, well-structured with `--core`, `--clean`, `--no-test` options
- Uses `JOBS` env var for parallel builds
- Properly separates core-only vs full builds

### `CMakeLists.txt`
- Good use of `option()` for feature flags
- System bgfx detection with fallback to bundled
- Proper `CMAKE_CXX_STANDARD 26` with `CMAKE_CXX_STANDARD_REQUIRED ON`
- `compile_commands.json` exported for LSP

### `cmake/shared.cmake`
- Lists all core source files explicitly
- Proper target alias (`gold::shared`)
- GCC < 9 `stdc++fs` linkage handled
- `find_package(Threads REQUIRED)` + `${CMAKE_DL_LIBS}` for module loading

### Issues
- `cmake_minimum_required(VERSION 3.16...4.2)` — upper bound on CMake version is unusual and may break with future CMake releases
- No `target_include_directories` for `3rdParty/` submodules in `shared.cmake` — relies on transitive includes which may be fragile

---

## Quick Stats

- **Health score: 81/100 (B)**
- 51,761 functions analyzed, 3,658 over cognitive complexity threshold
- All top complexity hotspots are in 3rdParty (freetype, bgfx, dav1d) — Gold's own code is moderate
- 431 naming findings, mostly typos (`"gaurd"` instead of `"guard"`)

---

## Recommendations (Priority Order)

### P0 — Fix before any release

1. Fix 6 syntax errors in `src/file.cpp:181-209` — add `(list args = {})` to all 6 method implementations
2. Remove duplicate `objData` definition — move to a shared header (e.g., `object_impl.hpp`) or keep only in `object.cpp` and use the forward-declared type
3. Fix `use_count() <= 0` → `<= 1` in `object.cpp:672` and `list.cpp:127`
4. Fix Mat4x4 translation bug — `var.cpp:1077,1122`: change second `getFloat(5)` to `getFloat(6)`

### P1 — High impact

5. Refactor `var.cpp` operators — extract type dispatch into helper functions. A template-based approach with a `switch(type)` calling a `visit` function would reduce 5000+ lines to ~200.
6. Add `lock_guard`/`unique_lock` RAII — replace all manual `lock()`/`unlock()` pairs in `promise.cpp` and `worker.cpp`
7. Uncomment or remove the lock in `getObject()` (`object.cpp:598`) — either protect the read or document why it's safe without locking
8. Fix `"gaurd"` typo — consistent naming improves readability and reduces confusion

### P2 — Medium impact

9. Add tests for worker/promise threading — test `useAllCores()` + `joinThreads()` path with multiple concurrent promises
10. Add tests for web module — server routes, database CRUD, dataStore backends, HTML generation
11. Add multi-level prototype inheritance tests — test grandparent -> parent -> child chains
12. Add concurrent access stress tests — test thread safety of var/list/object under contention
13. Add expression string tests — deep nesting like `"a.b[0].c.d[1].e"`

### P3 — Long-term

14. Reduce type enum — template-based `vec<N, T>` and `mat<N, M>` would collapse 48 vec types to 2 templates
15. Remove `using namespace std` from `types.hpp` — use explicit `std::` prefixes
16. Consider `std::variant` or `std::expected` — for `var` and error handling, though the current approach works
17. Add `operator==` content comparison — or document that object identity comparison is intentional and provide a separate `equals()` method
18. Add input size/depth limits to JSON/binary parsers to prevent DoS via deeply nested structures
19. Review symlink handling in `recursiveReadDirectory` — consider adding a config option to disable symlink following
