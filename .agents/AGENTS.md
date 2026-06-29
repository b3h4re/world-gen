# Agent Instructions

- Never run clang formatting tools in this repository, including `clang-format`, `run-clang-format.py`, or formatting commands that invoke clang-format indirectly.
- Do not edit files unless the user specifically asks for changes. For review, explanation, investigation, or planning requests, inspect and report only.

## Preferred Sources

- Prefer official Vulkan documentation when answering Vulkan questions:
  - https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html
  - https://docs.vulkan.org/
- Prefer current official CMake documentation for CMake behavior and commands:
  - https://cmake.org/cmake/help/latest/
- Prefer official project documentation for Qt, toml++, and stb questions before relying on secondary sources.

## Project Structure Guidelines

- Keep platform/windowing concerns under `src/window`.
- Keep input collection and input translation under `src/game/input`; do not put platform input helpers inside app orchestration files.
- Keep terrain generation, config use, generator ownership, async terrain jobs, and mesh-data construction in `src/apps/terrain_app_core.*` or lower-level terrain modules.
- Keep Vulkan device, swapchain, descriptor, render-object ownership, and render submission orchestration in renderer-facing modules such as `src/apps/terrain_app_renderer.*`, `src/device`, `src/swap_chain`, `src/pipeline`, and `src/renderer`.
- Keep current GUI/menu behavior in `src/apps/terrain_app_gui.*` or `src/game/ui`.
- Keep `src/apps/terrain_app.*` as a high-level coordinator. Avoid adding backend-specific helpers or large implementation details there.
- Prefer extending existing abstractions like `WindowSurface` and `InputReader` when adding or changing backends.
