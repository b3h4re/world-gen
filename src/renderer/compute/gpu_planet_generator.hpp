#pragma once

#include "device/lve_compute_device.hpp"
#include "renderer/compute/computer.hpp"
#include "renderer/compute/gpu_height_map.hpp"
#include "terrain/generators/3d/generator.hpp"
#include "terrain/generators/3d/generator_spec.hpp"

#include <memory>

namespace lve {

class GpuPlanetGenerator {
public:
    virtual ~GpuPlanetGenerator() = default;

    virtual void dispatch(
        GpuHeightMap& output,
        const wgen::Generator3dSpec& spec,
        wgen::SeedType seed) = 0;
};

class PerlinNoise3dGpuGenerator final : public GpuPlanetGenerator {
public:
    explicit PerlinNoise3dGpuGenerator(LveComputeDevice& device);

    void dispatch(
        GpuHeightMap& output,
        const wgen::Generator3dSpec& spec,
        wgen::SeedType seed) override;

private:
    Computer computer_;
};

std::unique_ptr<GpuPlanetGenerator> makeGpuPlanetGenerator(LveComputeDevice& device, wgen::Generator3dKind kind);

} // namespace lve
