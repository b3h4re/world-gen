# world-gen

A Vulkan-based terrain generation and rendering sandbox with a Qt controls panel and a live generator pipeline.

The app lets you build a pipeline of generators, edit each row in place, and switch between CPU and Vulkan compute where supported.

## Generators

The current UI exposes these generators:

- `Value noise`
- `Perlin`
- `Worley`
- `Simplex`

Each row in the pipeline is one generator layer. The row can be edited, reordered through the list model, or removed from the pipeline.

For generators that support octave-style layering, the settings dialog includes:

- `Octave` index
- `Lacunarity`
- `Persistance`

The current pipeline design treats each row as one octave layer. A pipeline containing rows for octave `0`, `1`, `2`, and so on is equivalent to the corresponding octave generator setup.

## Controls

Viewport controls:

- `WASD` pans the camera
- `Q` and `E` zoom
- `Escape` closes the app

Terrain controls:

- Double-click a generator row to open its settings
- Right-click a generator row to open the context menu
- Use `Open generator settings` to edit a row
- Use `Remove generator` to delete a row from the pipeline
- Use the add button to insert a new generator

## Settings

Generator settings are edited from the Qt dialog. Available fields depend on generator type.

Common fields:

- `Scale`
- `Compute` method, when supported by the selected generator

Perlin:

- `Dots per cell`

Worley:

- `Dots per cell`
- `points`
- `Power`

Simplex:

- `Dots per cell`

Octave-capable generators also expose:

- `Octave`
- `Lacunarity`
- `Persistance`

Pipeline changes update the active generator spec immediately. Terrain regeneration only happens when you explicitly trigger it from the controls.

## Planet height units

Planet radii and terrain heights use meters when `planet.height_mode` is
`"physical_meters"`. Each 3D generator's scale is its height amplitude in
meters, and its bias is also measured in meters. Geometry uses those physical
heights directly; color normalization is a separate display-only range.

Configuration files that predate `height_mode` are loaded as
`"legacy_normalized"`, preserving the previous frozen global normalization.
Set the mode explicitly to opt into stable physical-height authoring.

## Build

Requirements:

- CMake 3.20 or newer
- A C++20 compiler
- Vulkan SDK/runtime
- Qt 6 Widgets
- `glslc`

Build and run:

```sh
cmake -S . -B build
cmake --build build
./build/world_gen
```

The build compiles the shaders into `build/shaders` as part of the normal CMake workflow.
