# gold/ConwaysGameOfLife
### Conway's game of life example

[![License](https://img.shields.io/badge/license-Apache%202-blue)](LICENSE)

What is this?

A basic implementation of Conway's Game of Life on the
[gold framework](https://github.com/auRose94/gold) game engine: an entity
grid driven by a step function, colored through sprites on the GPU surface.
The grid dimensions come from the camera's pixel size, so run it with a
window (the engine's defaults).

## Build and run

```sh
cmake -S . -B build -DGOLD_BUILD_EXAMPLES=ON
cmake --build build --target ConwaysGameOfLife
./build/examples/conwaysGameOfLife/ConwaysGameOfLife
```

All code not in 3rdParty or explicitly stated otherwise (like files in js
or css) are Apache version 2.