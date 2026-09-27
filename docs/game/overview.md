# Game Module

The `gold::game` module provides a component-based engine for 3D and 2D games, built on top of the core framework.

## Features
- Component-based architecture
- bgfx rendering integration
- SDL3 window and input support
- Texture loading (2D/3D/Cube)
- Auto shader compilation
- `uiSurface`: an HTML/CSS document rendered to a texture and drawn on a
  world quad (a CRT on a desk, a shop terminal), with pointer-ray picking and
  click/hover events routed into the UI. See the
  [UI module](../ui/overview.md).

## Documentation
- [Render Backends](backends/render.md)
- [Window System](backends/window.md)
- [Input System](backends/input.md)
- [UI surfaces](../ui/overview.md)
