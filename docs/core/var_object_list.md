<!-- Agent note: Detailed API for var/object/list with prototype inheritance and examples. -->
# var / object / list API

<!-- Agent note: This file documents detailed API for var/object/list. -->

## var

Universal dynamic value container with copy-on-write.

### Construction
```cpp
var v1 = 42;              // int64
var v2 = 3.14;             // double
var v3 = "hello";          // string
var v4 = gold::vec3f(1,2,3);
```

### Accessors
```cpp
string s = v.getString();
int64_t i = v.getInt64();
double d = v.getDouble(0); // for vector element
v.setInt64(1, 99);
```

### Type checks
```cpp
v.isString(); v.isNumber(); v.isFloating(); v.isSigned();
v.isObject(); v.isList(); v.isBinary();
v.isVec2(); v.isVec3(); v.isVec4(); v.isQuat(); v.isMat3x3(); v.isMat4x4();
```

### varRef
```cpp
varRef ref = v["key"];   // object
varRef ref = v[0];        // list/index
v["name"] = "bob";
string n = v["name"];
```

## object

Ordered map with prototype inheritance.

```cpp
object o;
o.setString("name","bob");
o.setInt64("age",30);
string n = o.getString("name","default");
o.setParent(proto);
o["name"] = "alice";
string json = o.getJSON(true);
```

Prototype chain: `getParent()`, `setParent()`, `inherits()`. Expression access: `getExpression("child[0].name")`.

Static parsers: `parseURLEncoded`, `parseCookie`.

## list

Dynamic array with thread-safe per-method locking.

```cpp
list li;
li.pushInt64(1);
li.pushString("hi");
li[0] = 99;
string json = li.getJSON(true);
```

Methods: `push*`, `set*`, `get*` with defaults. Iterators: `begin()`, `end()`. Helpers: `isAllFloating()`, `isAllNumber()`, `isAllObject()`. Bulk: `assign(types, void*, size)`.

Iteration via iterators is NOT synchronized. Copy items first.

## Prototype inheritance example
```cpp
object proto;
proto.setString("type","user");
object obj;
obj.setParent(proto);
obj["name"] = "bob";
string t = obj.getString("type"); // inherits "user"
```

## Usage notes
- Copy-on-write: `var` copies share storage.
- Mutations via `varRef` write through parent.
- `getX(name, default)` provides fallback; `varRef` has no default.
- Prefer `jo`/`ja` builders and `v["key"]` access.

<!-- Agent note: End of var/object/list docs. -->
