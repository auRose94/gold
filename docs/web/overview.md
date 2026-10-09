# Web Module

The `gold::web` module provides a high-level abstraction for building web
services, similar to Express.js, with HTML rendering and a file-backed
document store.

## Features
- Express-like routing (per-method routes with pattern parameters)
- Swappable server transports: the server facade is transport-free —
  routes buffer as gold data and `start()` resolves a transport plugin
  via `config "transport"`. The real engine is `"lws"` (libgoldLws,
  built when the system libwebsockets package exists): HTTP, WebSocket
  routes, HTTPS, and graceful stop. `"uws"` remains as the vendored
  stopgap. See [plugins](../backends/plugins.md).
- HTML5 rendering with pragmatic templating
- File-based `dataStore` persistence
- The HTML/CSS parsers that also feed the `ui` module's renderer

## Server facade

Routes buffer onto the server as gold data until `start()` dispatches
them into the transport:

```cpp
auto serv = server(jo("host", "127.0.0.1", "port", 8080, "transport", "lws"));
serv.get({"/users/:id", func(...)});
serv.ws({"/echo", jo(
    "open", func(...),            // (sock) — push "welcome" via sock.send
    "message", func(...),         // (sock, data) — echo etc.
    "close", func(...))});        // (sock)
serv.start();                     // blocks on the transport loop
serv.stop();                      // from another thread: the loop exits
```

Route matching follows the uWS conventions gold grew with: `:id` segments
capture one path segment, a trailing wildcard (`/assets/*`) captures the
rest including slashes; unmatched requests fall through mounts, then the
`setErrorHandler` handler (the 404-page hook), then an automatic 404.

`start()` returns an error instead of looping when the bind fails.
Because `stop()` makes the loop return, the transport needs no
process-`_Exit` tricks and servers can be restarted inside one process.
On `"lws"`, `"sslCert"`/`"sslKey"` (a PEM pair) switch the vhost to
HTTPS on the same port.

## HTML5 Rendering

The HTML module provides both a **builder** and a **parser** for HTML5 elements.

### Builder API (JS-style)

Build HTML elements with the element classes; each ctor takes one `list` of:
child elements, strings (text), and attribute bundles (merged key by key). An
`items` key inside a bundle routes to the children instead of becoming an
attribute:

```cpp
#include "web/html.hpp"

using namespace gold;
using namespace HTML;

auto page = HTML::html(list({
    jo("lang", "en"),                       // attribute bundle
    HTML::head(list({
        HTML::title(list({"My App"})),
    })),
    HTML::body(list({
        HTML::h1(list({"Hello World"})),
        HTML::p(list({"This is a paragraph"})),
        HTML::div(list({
            jo("class", "container"),
            HTML::a(list({"Click me", jo("href", "/link")})),
        })),
    })),
}));

// Convert to HTML string: text and attribute values are escaped
// (entities like `&amp;`), except inside <script> / <style>, which
// round-trip their raw source.
string html = (string)page;
```

### Parser API

Parse HTML strings back into gold objects:

```cpp
#include "web/html.hpp"

using namespace gold;
using namespace HTML;

// Parse HTML string → list of iHTML elements
auto elements = Parser::parseHTML("<div class='foo'><p>Hello</p></div>");

// Access parsed elements
auto div = elements[0].getObject<iHTML>();
string tag = div["tag"];           // "div"
object attrs = div["attr"];       // {"class": "foo"}
list items = div["items"];        // [<p>Hello</p>]

// Round-trip: parse → modify → render
auto parsed = Parser::parseHTML("<p>Original</p>");
auto p = parsed[0].getObject<iHTML>();
p["items"] = ja("Modified text");
string html = (string)p;          // "<p>Modified text</p>"
```

### Parser Features
- Nested elements with full tree structure
- Attribute parsing (quoted, unquoted, boolean)
- Self-closing/void tags (`<br>`, `<img>`, `<input>`, etc.)
- Text nodes (mixed content support)
- HTML comment skipping
- Case-insensitive tag names
- Implicit closes (`<li>` inside `<li>`, `<p>` before a block element,
  table rows and cells, nested `<a>`)
- Character references: `&amp;` `&lt;` `&gt;` `&quot;` `&apos;` `&nbsp;`
  (plus a few others), decimal (`&#65;`) and hex (`&#x42;`) forms,
  decoded in text, `title`/`textarea` bodies and attribute values.
  `script`/`style` bodies stay raw. Unknown or truncated entities survive
  as written.
- Malformed input is recovered from, never fatal: unterminated tags,
  comments, raw text and entities all degrade to ordinary content

Two helpers round out the character-reference story:
`Parser::decodeEntities(text)` (parse-side) and
`Parser::escapeHTML(text, attribute)` (serialize-side).

### Void Tags
The parser recognizes these self-closing tags:
`area`, `base`, `br`, `col`, `embed`, `hr`, `img`, `input`, `link`, `meta`,
`param`, `source`, `track`, `wbr`

## CSS parsing

`CSS::parseCSS(text)` turns a stylesheet into a list of gold rule objects
(`selector` + typed `declarations` — see
[the ui module](../ui/overview.md) for the rendering front end,
`UI::parseStylesheet`, which adds `@media`/`@keyframes`/`@font-face`).

`!important` is recorded as a boolean under the key `name + "!"`, so values
stay typed: `parseDeclarations("margin: 0 !important")` yields `{"margin", 0}`
and `{"margin!", true}`.

## Data store

```cpp
database db({{"backend", "file"}, {"name", "mydb"}, {"path", "./data"}});
db.connect();

auto users = db.getCollection({"users"});
users.insert({jo("name", "alice", "tags", ja("admin", "user"))});
auto found = users.findMany({jo("tags", jo("$in", ja("admin")))});
users.updateOne({jo("name", "alice"), jo("$inc", jo("logins", 1))});
```

Updates support `$set`, `$unset` (remove fields) and `$inc` (add to a
numeric field); an update without any of them is an error rather than a
silent no-op. Inserts reject a duplicate `_id`; `replace` keeps the
matched document's `_id` (the file name and payload stay consistent), and
`addIndexes`/`dropIndex` are advisory bookkeeping on the "file" backend.

Body callbacks (`onData`) are invoked with `(buffer, request, response)`.

## Documentation
- [Data Store](../core/data_store.md)
- [Window System](../backends/window.md)
- [Input System](../backends/input.md)
