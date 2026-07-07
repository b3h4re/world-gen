#pragma once

#include "device/lve_compute_device.hpp"
#include "renderer/compute/gpu_height_map_accumulator.hpp"
#include "renderer/compute/gpu_planet_generator.hpp"
#include "terrain/generators/3d/generator_spec.hpp"
#include "terrain/planet.hpp"

#include <memory>
#include <vector>

namespace lve {

struct GpuPlanetGeneratorRequest {
    wgen::Generator3dSpec spec{};
    wgen::SeedType seed{};
};

class GpuPlanetPipeline {
public:
    GpuPlanetPipeline();

    GpuPlanetPipeline(const GpuPlanetPipeline&) = delete;
    GpuPlanetPipeline& operator=(const GpuPlanetPipeline&) = delete;

    wgen::Planet<float> generatePlanet(
        const std::vector<GpuPlanetGeneratorRequest>& requests,
        std::size_t dots);

private:
    GpuPlanetGenerator& generatorFor(wgen::Generator3dKind kind);

    LveComputeDevice device_{};
    GpuHeightMapAccumulator accumulator_;
    std::unique_ptr<GpuPlanetGenerator> perlinNoiseGenerator_{};
};

} // namespace lve
