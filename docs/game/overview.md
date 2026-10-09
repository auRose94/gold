# Game Module

The `gold::game` module provides a component-based engine for 3D and 2D games, built on top of the core framework.

## Features
- Component-based architecture
- bgfx rendering integration
- SDL3 window and input support
- Texture loading (2D/3D/Cube)
- Auto shader compilation
- glTF mesh rendering: `meshRenderer` draws a loaded `mesh` through the PBR
  shader (`src/shaders/pbr/`), with per-node transforms, material factors and
  textures, and one punctual light slot fed from `light` components
  (config `"unlit": true` on the component renders the flat texture — the
  classic display-model look); the PBR uniform bank travels as five mat4
  uniforms, which is what this system's bgfx delivers per draw
- `uiSurface`: an HTML/CSS document rendered to a texture and drawn on a
  world quad (a CRT on a desk, a shop terminal), with pointer-ray picking and
  click/hover events routed into the UI. See the
  [UI module](../ui/overview.md).

## Debugging
- Engine config `"screenshot": "/path/to/shot.png"` makes the graphics backend
  write a PNG of the back buffer at frame 16 (CI/agents can then verify a
  render without reading the screen).
- Config `"graphics": {"debug": true}` enables bgfx's debug overlay/stats.

## Documentation
- [Render Backends](backends/render.md)
- [Window System](backends/window.md)
- [Input System](backends/input.md)
- [UI surfaces](../ui/overview.md)
