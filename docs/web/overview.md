# Web Module

The `gold::web` module provides a high-level abstraction for building web services, similar to Express.js, with native support for HTTP(S), WebSockets, and HTML5 rendering.

## Features
- Express-like routing and middleware
- Native HTML5 rendering with pragmatic templating
- File-based `dataStore` persistence
- WebSocket support

## HTML5 Rendering

The HTML module provides both a **builder** and a **parser** for HTML5 elements.

### Builder API (JS-style)

Build HTML elements using the fluent, JS-like syntax:

```cpp
#include "web/html.hpp"

using namespace gold;
using namespace HTML;

// Create elements with jo/ja sugar
auto page = jo("tag", "html",
    "attr", jo("lang", "en"),
    "items", ja(
        jo("tag", "head",
            "items", ja(
                jo("tag", "title", "items", ja("My App"))
            )
        ),
        jo("tag", "body",
            "items", ja(
                h1("Hello World"),
                p("This is a paragraph"),
                div(jo("class", "container"),
                    jo("items", ja(
                        a("Click me", jo("href", "/link"))
                    ))
                )
            )
        )
    )
);

// Convert to HTML string
string html = (string)page.getObject();
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

### Void Tags
The parser recognizes these self-closing tags:
`area`, `base`, `br`, `col`, `embed`, `hr`, `img`, `input`, `link`, `meta`, `param`, `source`, `track`, `wbr`

## Documentation
- [Data Store](core/data_store.md)
- [Window System](backends/window.md)
- [Input System](backends/input.md)
