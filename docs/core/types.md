# Core Types System

<!-- AGENT NOTE: This document describes the type system for Gold. Key concepts: var is a type-erased container with copy-on-write semantics via shared_ptr. types_t enum defines all supported types. varRef provides JS-style property access. -->

## Overview

Gold's type system centers on `var`, a universal dynamic value container. Values are copy-on-write via `shared_ptr<varContainer>`, making passing by value cheap. The system supports scalars, strings, binary, lists, objects, methods/functions, and advanced math types (vectors, quaternions, matrices).

## types_t Enum

```cpp
typedef enum types_t {
    typeNull = 0,
    typeList,
    typeObject,
    typeMethod,
    typeFunction,
    typePtr,
    typeString,
    typeStringView,
    typeBinary,
    typeInt64,
    typeInt32,
    typeInt16,
    typeInt8,
    typeUInt64,
    typeUInt32,
    typeUInt16,
    typeUInt8,
    typeDouble,
    typeFloat,
    typeBool,
    typeException,
    // Advanced math
    typeVec2Int64, typeVec2Int32, typeVec2Int16, typeVec2Int8,
    typeVec2UInt64, typeVec2UInt32, typeVec2UInt16, typeVec2UInt8,
    typeVec3Int64, typeVec3Int32, typeVec3Int16, typeVec3Int8,
    typeVec3UInt64, typeVec3UInt32, typeVec3UInt16, typeVec3UInt8,
    typeVec4Int64, typeVec4Int32, typeVec4Int16, typeVec4Int8,
    typeVec4UInt64, typeVec4UInt32, typeVec4UInt16, typeVec4UInt8,
    typeVec2Float, typeVec2Double,
    typeVec3Float, typeVec3Double,
    typeVec4Float, typeVec4Double,
    typeQuatFloat, typeQuatDouble,
    typeMat3x3Float, typeMat3x3Double,
    typeMat4x4Float, typeMat4x4Double,
} types;
```

Helper: `getTypeString(types)` returns human-readable names.

## var Container

`var` is the primary value type. It holds any gold type through type erasure.

**Key characteristics:**
- Copy-on-write via `shared_ptr<varContainer>`
- `var()` creates null
- Implicit conversions to string, explicit to string_view
- Arithmetic operators overloaded for scalars and math types
- `getType()`, `getTypeString()`, type checkers (`isString()`, `isNumber()`, etc.)

**Construction examples:**
```cpp
var v1 = 42;              // int64
var v2 = 3.14;             // double
var v3 = "hello";          // string
var v4 = gold::vec3f(1,2,3); // Vec3Float
```

**Accessors:**
```cpp
string s = v.getString();
int64_t i = v.getInt64();
double d = v.getDouble(0); // for vector element
v.setInt64(1, 99);       // mutate vector element
```

**JS-style access:**
```cpp
varRef ref = v["key"];   // object
varRef ref = v[0];        // list/index
v["name"] = "bob";       // write
string n = v["name"];     // read
```

**Type checks:**
```cpp
v.isString(); v.isNumber(); v.isFloating(); v.isSigned();
v.isObject(); v.isList(); v.isBinary();
v.isVec2(); v.isVec3(); v.isVec4(); v.isQuat(); v.isMat3x3(); v.isMat4x4();
```

<!-- AGENT NOTE: var is copy-on-write. Do not assume deep copy on assignment. Mutations via varRef may trigger copy-on-write internally? Actually varRef assigns directly to container. Be careful with shared state. -->

## varRef

`varRef` is a proxy for read/write access to object properties or list indices.

**Features:**
- Constructed via `var::operator[]` for string_view or integral index
- Nested access: `v["a"]["b"]`, `v["a"][0]`
- Assignment operators for all scalar types, list, object, binary
- Conversion operators to `var`, `string`, `int64_t`, `double`, `bool`
- Mirrors `var` read API: `getType()`, `isString()`, `getString()`, etc.
- Chainable `set(name, value)`

**Usage:**
```cpp
object user;
user["name"] = "Alice";
user["age"] = 30;
user["tags"] = list({"admin", "dev"});

string name = user["name"];          // read
user["age"] = user["age"] + 1;     // write
user["profile"]["email"] = "a@b";   // nested
```

<!-- AGENT NOTE: varRef holds a parent var by value. Assignment writes through parent container. For safe nested writes, ensure parent is not null. -->

## object

Ordered map of named `var` values with prototype-based inheritance.

**Features:**
- `map<key, var>` storage with shared_ptr
- Prototype chain via `setParent()` / `getParent()` / `inherits()`
- Expression access: `getExpression("child[0].name")`
- JSON/BSON/CBOR/MsgPack/UBJSON serialization
- `operator[]` returns `varRef`
- Static parsers: `parseURLEncoded`, `parseCookie`

**Methods:**
```cpp
object o;
o.setString("name", "bob");
o.setInt64("age", 30);
string n = o.getString("name", "default");
o.setParent(proto);
o["name"] = "alice";  // JS-style
string json = o.getJSON(true);
```

**Creation helpers:**
```cpp
auto o = object({{"name","bob"},{"age",30}});
o.create("child", config);
```

<!-- AGENT NOTE: object data is shared via shared_ptr. Copying object shares data. Use copy constructor for shallow copy. -->

## list

Dynamic array of `var` values with serialization support.

**Features:**
- Thread-safe per-method locking
- `push*`, `set*`, `get*` with defaults
- JSON/BSON/CBOR/MsgPack/UBJSON serialization
- `operator[]` returns `varRef` for non-const access
- Iterators: `begin()`, `end()`, `rbegin()`, `rend()`
- `isAllFloating()`, `isAllNumber()`, `isAllObject()`
- `assign(types, void*, size)` for bulk copy

**Usage:**
```cpp
list li;
li.pushInt64(1);
li.pushString("hi");
li[0] = 99;  // varRef
string json = li.getJSON(true);
```

**Note:** Iteration via iterators is NOT synchronized. Copy items first for safe read.

<!-- AGENT NOTE: list::avec::iterator is raw vector iterator. Do not mutate list while iterating. -->

## binary

Type alias: `using binary = vector<uint8_t>;`

Used for binary blobs, serialization output, and `var` storage.

## Advanced Math Types

Vectors, quaternions, matrices are stored as raw arrays in `varContainer` with type-specific enum values.

**Construction helpers:**
```cpp
var v = vec2i64(1,2);
var v = vec3f(1.0f,2.0f,3.0f);
var v = vec4d(1.0,2.0,3.0,4.0);
var v = quatf(0,0,0,1);
var v = mat4x4f({1,0,0,0, ...});
```

**Supported types:**
- Vec2/Vec3/Vec4 for Int8/16/32/64, UInt8/16/32/64, Float, Double
- QuatFloat, QuatDouble
- Mat3x3Float/Double, Mat4x4Float/Double

**Operations:**
Arithmetic operators `+ - * / %` are overloaded for math types:
- Vector arithmetic element-wise
- Quaternion multiplication with vector (rotation) and quaternion
- Matrix multiplication, vector multiplication, quaternion to matrix conversion
- `operator-()` negates components

**Access:**
```cpp
var v = vec3f(1,2,3);
float x = v.getFloat(0);
v.setFloat(1, 5.0f);
```

<!-- AGENT NOTE: Math types use raw new[] allocation in varContainer. Destructor uses deleteTypeB for array deallocation. Ensure correct type matching. -->

## genericError

Error value carrying message + file/function/line.

```cpp
genericError err("msg", __FILE__, __FUNCTION__, __LINE__);
var v = err;
bool isErr = v.isError();
genericError* e = v.getError();
string msg = (string)e;
```

Returned as `var` rather than thrown. Check with `isError()`.

## Usage Notes

- **Copy-on-write:** `var` copies share storage. Mutations via `varRef` write through shared container. For isolation, create new var.
- **Conversions:** `getString()` works for all types. `operator string()` implicit. `operator string_view()` explicit.
- **Defaults:** `getX(name, default)` methods provide fallbacks. `varRef` has no default.
- **JS ergonomics:** Prefer `jo(...)`/`ja(...)` builders and `v["key"]` access. See `include/goldjs.hpp`.
- **Thread safety:** `list` methods lock individually. `object` methods are not explicitly locked in header; assume single-thread or external sync.
- **Serialization:** `object` and `list` support multiple formats. Use `getJSON()`, `getBSON()`, etc.

## Code Examples

**Basic var usage:**
```cpp
var v = 42;
int64_t i = v;          // implicit
string s = (string)v;   // "42"
v = v + 1;
```

**Object with prototype:**
```cpp
object proto;
proto.setString("type", "user");
object obj;
obj.setParent(proto);
obj["name"] = "bob";
string t = obj.getString("type"); // inherits
```

**List serialization:**
```cpp
list li = ja(1,2,3);
string json = li.getJSON(true);
```

**Math:**
```cpp
var v = vec3f(1,0,0);
var q = quatf(0,0,0.707,0.707);
var r = q * v; // rotated vector
```

<!-- AGENT NOTE: This file is for documentation. Keep examples minimal and correct. Avoid assumptions about API changes. -->

