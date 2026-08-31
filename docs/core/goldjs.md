# JavaScript-style Ergonomics

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
