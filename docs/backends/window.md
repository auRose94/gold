# Window system backends

Window creation is abstracted behind `windowSystem` (a pure interface with no
SDL/bgfx types in its headers). The `window` object is a facade over a
backend chosen by name:

* `"sdl"` / `"sdl3"` — SDL3 window + input events (registered under both
  names). SDL3 ships Wayland and X11 drivers (native handles come from SDL
  window properties; on Wayland gold creates the `wl_egl_window` the EGL
  render backend needs).
* `"wayland"` — native Wayland (xdg-shell) window + wl_seat input.
* `"headless"` — no real window; for tests, CI, and offscreen rendering.

Backends are registered via `registerWindowSystem()`. The `"backend"` config
accepts a string or a list of names tried in order (fallback chain);
`"headless"` is always appended last so apps never hard-fail on a missing
compositor:

```json
{ "backend": ["wayland", "sdl", "headless"] }
```
