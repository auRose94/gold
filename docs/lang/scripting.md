# Scripting Language (`gold::lang`)

A TypeScript-like interpreter whose runtime IS gold: values are `var`/`object`/
`list`, the AST is built from gold objects, and scopes reuse gold's prototype
chain. Type annotations are optional and can be enforced at runtime.

```cpp
auto r = gold::langRun(
  "class Point { constructor(x, y) { this.x = x; this.y = y; } "
  "  len() { return this.x * this.x + this.y * this.y; } } "
  "const p = new Point(3, 4); p.len();", gold::object(), false);
// r == 25
```

The `script` object embeds a script in a host app: `setGlobal`/`getGlobal`
share gold objects with the host, `eval` evaluates expressions, and `call`
invokes functions — so scripts can use gold backends directly.
