<!-- Agent note: JS-style ergonomics for Gold. Covers o/a builders, varRef access, template strings, array helpers, toJSON/fromJSON. Used for documentation and AI agent context. -->
# Gold JS-Style Ergonomics

<!-- Agent note: This file documents goldjs.hpp helpers. Keep examples accurate to jo/ja/tpl/each/mapArr/filter/findArr/join/toJSON/fromJSON. -->

## Overview

`include/goldjs.hpp` provides JavaScript/TypeScript-style sugar for building objects/arrays, property access, template strings, array helpers, and JSON shorthand. Property read/write through objects/lists comes from `types.hpp` via `varRef` (`v["key"]` / `v[i]`).

## Object/Array Literals

Build objects with `jo` and arrays with `ja`. These are nestable.

```cpp
auto user = jo(
  "name", "bob",
  "age", 30,
  "tags", ja("admin", "dev"),
  "stats", jo("age", 30, "score", 9.5)
);
```

`jo` signature: `jo(const char* key, V&& value, R&&... rest)` → `object`
`ja` signature: `ja(V&& value, R&&... rest)` → `list`

Usage notes:
- `jo` builds an `object` by setting vars in order.
- `ja` builds a `list` by pushing vars.
- Both are nestable and variadic.

## varRef Property Access

Read/write through `varRef` proxies:

```cpp
user["age"] = 31;            // write
user["score"] = 10;          // write
user["tags"][0] = "staff";   // nested list write
string name = user["name"];   // read
int64_t age = user["age"];   // implicit conversion
```

Nested access works: `v["a"]["b"]`, `v["a"][0]`. `varRef` supports assignment for scalars, `list`, `object`, `binary`.

## Template Strings

```cpp
string s = tpl("Hello $0, you scored $1", user["name"], user["score"]);
// "Hello bob, 10"
```

`tpl(const char* format, A&&... args)` replaces `$0`, `$1`, ... with stringified args.

## Array Helpers

- `each(list li, const func& fn)` – call fn(element) for each element
- `mapArr(list li, const func& fn)` – return list of fn results
- `filter(list li, const func& fn)` – keep elements where fn is truthy
- `findArr(list li, const func& fn)` – first matching element or empty var
- `join(list li, string_view sep = ",")` – concatenate as strings

Example:
```cpp
auto nums = ja(1,2,3,4,5);
auto doubled = mapArr(nums, func([](list a){ return var(a[0].getInt64()*2); }));
// [2,4,6,8,10]
auto evens = filter(nums, func([](list a){ return var(a[0].getInt64()%2==0); }));
string s = join(doubled); // "2,4,6,8,10"
```

## JSON Shorthand

```cpp
string json = toJSON(value, pretty = false);
var v = fromJSON(string_view data);
```

Wrappers for `jsonStringify` / `jsonParse`. Returns `genericError` var on parse failure.

## Usage Notes

- Prefer `jo`/`ja` over manual `object({{"k",v}})` / `list({...})`.
- `varRef` has no default fallback; use `getX(name, default)` for safe reads.
- Template strings use `$0` indexing, not named placeholders.
- Array helpers accept `func` callbacks receiving a `list` with one element.

<!-- Agent note: End of goldjs documentation. Keep in sync with include/goldjs.hpp. -->
