# The MCP debug server (module `mcp`)

Gold can host an **MCP (Model Context Protocol) server** inside a running
app: an MCP client — an IDE, an AI agent, or any streamable-HTTP MCP
client — connects over localhost HTTP and inspects/modifies the live
gold data that *is* the app: entity trees, the UI DOM, window/input
state, physics bodies, script globals.

## Design in one paragraph

The server ships as the loadable **`libgoldMcp` module** (built when the
web module + the lws transport are built). The engine owns only a thin
seam — `include/game/mcpServerSystem.hpp` (`createMcpServer`/`registerMcpServer`,
the standard registered-backend pattern) — and loads the module when the
config asks for it. The endpoint is a JSON-RPC 2.0 route (`POST /mcp`)
on gold's own web `server` facade (the `lws` serverTransport plugin by
default), so nothing new is linked and no new network stack exists.

Tool calls execute on **the engine frame**: the endpoint queues requests
and `engine::start()` drains them every frame at the loop's safe point
(after the window pump, before physics/render — the previous frame has
fully rendered). Apps without an engine run tool calls inline on the
endpoint thread (config `execute` chooses; `auto` does the right thing).

## Activation

`config.json` (defaults shown; only `enabled` matters):

```json
{
  "mcp": {
    "enabled": false,
    "host": "127.0.0.1",
    "port": 8090,
    "transport": "lws",
    "eval": true,
    "execute": "auto",
    "timeout": 10000
  }
}
```

| key | meaning |
| --- | --- |
| `enabled` | Gate. The engine logs and continues when the module is missing. |
| `host`/`port` | The endpoint binds here. **Default 127.0.0.1 — the server performs arbitrary state mutation and (with `eval`) arbitrary script execution; do not expose it beyond localhost (no auth in this version).** |
| `transport` | `createServerTransport` name (the loadable libgoldLws by default). |
| `eval` | `false` removes the `eval` tool from the catalog. |
| `execute` | `auto` (frame as soon as the engine pumps; inline before that) · `frame` (queue until a pump; times out per `timeout`) · `server` (always inline). |
| `timeout` | How long a frame-queued request waits (ms) before a 503. |

Console activation overrides the file (defaults < `config.json` < argv):

```bash
app --mcp=on          # enable with the configured port
app --mcp=on --mcp-port=8090
app --mcp=8090        # number: enable at that port
app --mcp=off         # disable (overrides the file)
```

The endpoint comes up during engine construction and listens from then
on; requests arriving before the loop starts queue until the first
frame's pump.

## Pointing a client at it

Any streamable-HTTP MCP client works; there is no session id, so a
client config like:

```
http://127.0.0.1:8090/mcp
```

is all it needs. Handshake: `initialize` (negotiates `2025-06-18`,
reports the tool catalog) → `notifications/initialized` (202) →
`tools/list`, `tools/call`, `ping`. `GET /mcp` answers 405 (no SSE
response stream yet), `DELETE /mcp` is accepted and idempotent.

## The tool catalog

| tool | what it does |
| --- | --- |
| `state_get` | Read gold data at a path (`engine.entities.0.name`, `engine.window.title`); empty path lists roots. Binaries/functions read as null; oversized serializations are refused with guidance; `depth` caps nesting. |
| `state_list` | Key/type listing of a value (cheap browse before reads). |
| `state_set` | Write an existing member. Uniform number arrays land as gold vectors (`[1,2,3]` → vec3f), matching how the app reads them. New members are created via `eval`, not here. |
| `call` | Invoke a method on the object at a path (`engine.world`, `engine.graphics`, a dataStore...) with positional args. |
| `eval` | Run gold::lang with the attached roots as globals — the same live objects (shared handles): scripts read/write app state in place, call methods, spawn entities. |
| `entities_list` | Compact walk of `engine.entities`: names, enabled, components with best-effort prototype names, children. Indices match the paths above. |
| `ui_query` | CSS selectors against a uiSurface's DOM (element descriptors); empty selector → the full markup + frame counters. |
| `ui_set` | Element mutations: text, style declarations, pseudo-class states; forces the re-render. |
| `ui_event` | Synthetic UI pointer events through the same dispatch pipeline as real input. |
| `screenshot` | Two-phase by necessity (backends read back post-submit): first call requests, the next call serves the PNG as MCP **image content** (+text path) and removes the file so the next capture is fresh. Default `/tmp/gold-mcp.png`. |
| `window_input` | Inject device-shaped events (`key_down`/`text_input`/`mouse_move`/...) through `window.handleEvent` — indistinguishable downstream from real input. |

Path grammar: `root.key.index.key…` — roots are the names the engine
attached (`engine`) plus whatever the app attached
(`mcpServer.attach(name, object)` / `setRoots(object)` for facade users).
Errors quote the available roots/members so recovery is one step.

## Facade-direct users

Apps can host the endpoint without an engine — link `gold::mcp`:

```cpp
mcpServer devServer(jo("host", "127.0.0.1", "port", 8090));
devServer.setRoots(jo("app", appObject));
devServer.start();   // own thread; stop() from anywhere
```

## Limits of this version

- One implicit session, single client at a time; no `Mcp-Session-Id`
  (libwebsockets exposes arbitrary headers only through its token
  table), no SSE.
- No server-initiated notifications (`resources`/`logging` capabilities).
- No pause/step-frame: `state_set engine.running=false` ENDS the app
  (pause needs an engine-side onFrame hook someday).
- Calling facade methods with raw C++ members (`uiSurface::draw` and
  friends) through gold-handle copies is unsafe in general — the tools
  route around the renderer's C++-only surface via uiSurface's gold
  methods; `call` on a running engine is for data-level methods.
- No auth: bind to 127.0.0.1 (the default) and treat the endpoint as
  trusted usercode access.