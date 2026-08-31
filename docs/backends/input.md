# Input backends

`inputSystem` abstracts device capture. Backends:

* `"evdev"` — real device capture via libevdev (`/dev/input/event*`).
* `"sdl"` — SDL3's unified input: gamepad (`gamepad_button`/`gamepad_axis`/
  `gamepad_touchpad` events), touch (`touch_down`/`touch_move`/...), and
  sensors (`sensor`). Keyboard/mouse continue to arrive through the window
  event stream; the SDL input backend re-pushes those events so the window
  backend still sees them.
