# Render backends

The render backend binds to the platform window through `windowSystem::native()`.
Select it with the graphics config `"renderBackend"` (`"bgfx"` default,
`"sdlgpu"` for the SDL3 GPU backend, `"vulkan"` reserved); it falls back to
bgfx when unavailable. The same selection is reachable from the launch
command on an engine constructed with `argc`/`argv`: `--render-backend=NAME`
picks the backend and `--renderer=NAME` sets the renderer-API hint. The SDL3
GPU backend currently drives the window's
swapchain (device init, clear, present); the full resource/draw pipeline is
in progress.

The engine loop runs at a configurable frame rate (`"frameTime"` ms in the
game's `config.json`, default 16 → 60fps) so it does not peg the CPU when
the compositor does not present/vsync.
