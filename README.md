# world-gen

A Vulkan-based 2D terrain generation and rendering sandbox.

The Vulkan platform layer follows the structure of `vulkan-nameless-game`. Code
that differs specifically for 2D rendering is grouped under `src/game/2d`.

## Build

Requirements: CMake, Vulkan SDK/runtime, GLFW, GLM, and `glslc`.

```sh
cmake -S . -B build
cmake --build build
./build/world_gen
```

Use `WASD` to pan, `Q`/`E` to zoom, and `Escape` to close.
