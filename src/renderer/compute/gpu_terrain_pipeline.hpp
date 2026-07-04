#pragma once

#include "device/lve_compute_device.hpp"
#include "renderer/compute/gpu_generator.hpp"
#include "renderer/compute/gpu_height_map_accumulator.hpp"
#include "terrain/generators/generator_spec.hpp"
#include "terrain/terrain.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace lve {

struct GpuGeneratorRequest {
    wgen::GeneratorSpec spec{};
    wgen::SeedType seed{};
};

class GpuTerrainPipeline {
public:
    GpuTerrainPipeline();

    GpuTerrainPipeline(const GpuTerrainPipeline&) = delete;
    GpuTerrainPipeline& operator=(const GpuTerrainPipeline&) = delete;

    wgen::HeightMap<float> generateHeightMap(
        const std::vector<GpuGeneratorRequest>& requests,
        std::size_t width,
        std::size_t height);

private:
    GpuGenerator& generatorFor(wgen::GeneratorKind kind);

    LveComputeDevice device_{};
    GpuHeightMapAccumulator accumulator_;
    std::unique_ptr<GpuGenerator> valueNoiseGenerator_{};
    std::unique_ptr<GpuGenerator> perlinNoiseGenerator_{};
    std::unique_ptr<GpuGenerator> worleyNoiseGenerator_{};
    std::unique_ptr<GpuGenerator> simplexNoiseGenerator_{};
    std::unique_ptr<GpuGenerator> waveletNoiseGenerator_{};
    std::unique_ptr<GpuGenerator> gaborNoiseGenerator_{};
};

} // namespace lve
