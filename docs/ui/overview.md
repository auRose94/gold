# UI Module

`gold::ui` turns HTML and CSS into pixels. It has no GPU dependency, so it
runs headless (in tests, in a server, in a build step) and the engine module
adds a thin adapter on top for in-world surfaces.

The design goal is a **UI that is cheap to keep on screen**: the pipeline is
staged and dirty-tracked, so a static document costs one pass at load and
nothing per frame, and only the stages that actually changed re-run.

```
HTML ──parse──▶ DOM ──cascade──▶ computed styles ──layout──▶ box tree ──paint──▶ RGBA8
                    ▲                    ▲                    │                  │
                 hover/focus        animations           hit test          upload to
                 setState()         transitions           pointer ray      a texture
```

## Quick start

```cpp
#include "ui/software_renderer.hpp"
using namespace gold;

UI::software_renderer ui;   // concrete CPU-rasterizer backend
ui.load(list({jo("html", markup, jo("css", sheet),
    jo("width", 512.0), jo("height", 384.0))}));

// One-time, after loading and whenever something changes:
if (ui.needsRender()) ui.render();     // -> bool: pixels changed

// The pixels (straight-alpha RGBA8, top-down):
binary pixels = ui.target().unpremultiply();
```

`renderer` is a gold object, so the same instance is drivable from C++, from a
`gold::lang` script, or from game code.

## Per-frame cost

| Situation | Work |
| --- | --- |
| Nothing changed | one `bool` check |
| `setText` / `setStyle` / `setHTML` | style + layout + paint |
| Pointer moved onto a new element | style (hover moved) + layout + paint |
| Viewport resized | style + layout + paint |
| CSS animation running | style + layout + paint, every frame |

`ui.stats()` returns paint / layout / style pass counts and timings for a debug
HUD.

## Styling

Supported: the full cascade (UA defaults, specificity, `!important`, inline
`style`), selectors (type, `*`, `.class`, `#id`, `[attr]` with all operators,
`:not()`, `:hover`, `:active`, `:focus`, `:checked`, `:disabled`, `:enabled`,
`:first-child`/`:last-child`/`:only-child`, `:first-of-type`/`:last-of-type`,
`:nth-child()`/`:nth-last-child()`/`:nth-of-type()`, `:root`, `:empty`),
combinators (descendant, `>`, `+`, `~`), `@media` (viewport size and
`hover`/`pointer` capabilities), and `@keyframes` parsing (blocks are
collected; see *Not yet implemented*).

Properties cover the box model (`width`/`height`/`min-*`/`max-*`, `margin`,
`padding`, `border` and its per-side shorthands, `border-radius`,
`box-sizing`), `display` (including `flex` and `grid`), positioning
(`position`, `top`/`right`/`bottom`/`left`, `z-index`), backgrounds (colors,
`linear-gradient`, `radial-gradient`, `background-size`), text (family, size,
weight, style, line-height, letter-spacing, align, decoration, indent,
white-space, vertical-align), `overflow` (clipping), `opacity`, `visibility`,
`pointer-events`, and the flex/grid property sets.

`length` supports `px`, `pt`, `em`, `rem`, `%`, `vw`, `vh` and `auto`.

## Layout

- **Block and inline formatting contexts.** Block children stack vertically,
  inline content breaks into line boxes with white-space collapsing, greedy
  line breaking, and `text-align` (including `justify`).
- **Shrink-to-fit sizing** for inline-block / flex / grid items, using both
  max-content and min-content widths.
- **Flexbox**: direction, wrap, gap, `justify-content`, `align-items` /
  `align-self` / `align-content`, `order`, and `flex-grow` / `flex-shrink` /
  `flex-basis`.
- **Grid**: `grid-template-columns` / `grid-template-rows` with `px`, `fr`,
  `auto`, `minmax()` and `repeat()`, auto tracks sized to content,
  `grid-auto-flow`, and `grid-column` / `grid-row` spans (`2 / span 2`).
- **Out-of-flow children**: `position: absolute` is laid out against the
  containing block without affecting its height; `position: relative` offsets
  the box after layout.

Known simplifications: margin collapsing is limited to adjacent siblings (a
parent's top margin and its first child's are not collapsed), and block-level
boxes nested inside an inline formatting context are treated as atomic items.

## Text and fonts

```cpp
ui.setFonts(list({jo("sans", "assets/Inter-Regular.ttf",
                     jo("mono", "assets/InterMono.ttf")}));
ui.loadFont(list({"assets/Inter-Bold.ttf", "sans-serif", 700.0}));
```

Font files register under the family names they answer to, including the
generic CSS families (`sans-serif`, `serif`, `monospace`). Glyphs are
rasterized through FreeType with a small bitmap cache and horizontal
sub-pixel positioning.

**No font file required**: when a family has no file (or FreeType is
unavailable), a built-in 5x7 bitmap font takes over, so the renderer always
produces pixels and tests stay deterministic. Codepoints outside its range
draw a placeholder box.

## Interaction

Hit testing uses the same box tree the renderer paints, so picking and
painting can never disagree.

```cpp
// What is under this point?
object element = ui.hit(list({x, y}));
// {id, tag, cssId, class, text, attrs, rect, content, visible}

// React to a click anywhere, or only on one element.
ui.on(ja("click", func([](list args) { /* args[0] = element */ })));
ui.on(ja("click", elementId, func([](list args) { /* ... */ })));

// Drive the UI from a game loop.
ui.dispatch(list({"move", x, y}));   // hover follows the pointer
ui.dispatch(list({"down", x, y}));   // :active
ui.dispatch(list({"up", x, y}));     // fires "click" when released on target
ui.dispatch(list({"leave"}));        // clears hover, cancels a press

// Or set state directly (a game-controlled highlight, a locked button).
ui.setState(list({elementId, "hover", true}));
```

Handlers receive the element descriptor and bubble from the target up through
its ancestors; a document-level handler (`on("click", func)`) sees every event.
`query("li.item")` returns descriptors, and `elementRect(id)` gives a rect in
UI pixels for anchoring 3D widgets to a UI element.

## Editing the UI from code

```cpp
ui.setText(list({elementId, "SYSTEM READY"}));
ui.setStyle(list({elementId, jo("background", "#ff0000")}));
ui.setHTML(list({markup}));     // markup string, or builder elements
ui.markup();                    // serialize back to HTML (escaped)
```

`setHTML` accepts markup text, a single builder element
(`HTML::div(list({...}))`), or a list of them — so a document assembled with
the `gold::HTML` element classes renders directly. `setCSS` accepts a
stylesheet string or a list of `CSS::Rule` objects (the `CSS::parseCSS`
output), for building styles from gold data.

Gold DOM objects are shared, not copied, so mutating a parsed element
directly is also visible to the renderer: the next `render()` notices the
change through a fingerprint (shape, ids, text and attributes) and
re-resolves style and layout. `dirty()`/`needsRender()` report those
external mutations too.

Text and attribute values decode character references on parse (`&amp;`,
`&#65;`, `&#x42;`, ...) and escape them on serialize, except `script` and
`style` bodies, which round-trip raw source. See the web module docs for the
parser's full character-reference rules.

## In a 3D scene

`gold::game` ships `uiSurface`, a component that owns a renderer, uploads its
pixels to a texture, and draws it on a quad:

```cpp
auto screen = uiSurface(jo("html", markup, jo("css", sheet),
    jo("width", 512.0), jo("height", 384.0), jo("size", vec2f(0.32f, 0.24f))));

screen.on(ja("click", func([](list args) {
    log(args[0].getObject().getString("cssId"));
    return var();
})));

// Per frame, from the engine's draw list.
screen.draw(list({view}));

// Input: a world-space ray from the player's crosshair.
screen.pointer(list({"move", origin, direction}));
```

The ray is intersected with the quad's plane, converted to UI pixels, and
dispatched to the element under it. The texture is only re-uploaded when the
UI changed. See `include/game/uiSurface.hpp`.

## Not yet implemented

- CSS animations and transitions: `@keyframes` blocks are parsed and exposed
  (`stylesheet::findKeyframes`), and `advance(dt)` is the hook a running
  animation drives, but nothing plays them back yet. Animating
  layout-affecting properties (which is what the dirty-tracking design
  expects) is the next step.
- `::before` / `::after` are parsed and then ignored.
- `transform` (2D/3D), `float`, `columns`, tables, `float`-based pagination.
- `background-image: url(...)` reaches the rasterizer through
  `imageProvider`, but no provider is wired up by default.
- `color` keywords are a practical subset (CSS basic colors plus common
  names), not all 148.
- Per-corner `border-radius` values collapse to the largest radius, because
  the rasterizer's rounded-box distance field carries a single radius.
- Element `opacity` multiplies into the colors a box paints; nested opacity
  groups are not isolated into offscreen buffers.

## Tests

`tests/uiTest.cpp` (`goldUITests`) covers the tree, the cascade and selector
matching, layout geometry, rasterized pixels, dirty tracking, and the
interaction model. Pointer-ray projection for `uiSurface` is covered in
`tests/gameTest.cpp`.
