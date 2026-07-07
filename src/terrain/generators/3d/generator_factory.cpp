#include "generator_factory.hpp"

#include "terrain/generators/3d/noise/perlin.hpp"

#include <stdexcept>

namespace wgen {

std::unique_ptr<Generator3d> makeGenerator3d(const Generator3dSpec& spec, SeedType seed) {
    switch (spec.kind) {
        case Generator3dKind::PerlinNoise: {
            const auto* config = std::get_if<PerlinNoise3dGeneratorSpec>(&spec.config);
            if (config == nullptr) {
                throw std::invalid_argument("3D perlin noise generator spec has wrong config type");
            }

            return std::make_unique<PerlinNoise3d>(seed, config->cellSize);
        }
    }

    throw std::invalid_argument("unknown 3D generator kind");
}

std::unique_ptr<Generator3d> makePipelineGenerator3d(const Generator3dSpec& spec, SeedType seed) {
    return makeGenerator3d(spec, seed);
}

std::unique_ptr<TerrainPipeline3d> makePipeline3d(const Generator3dPipelineSpec& specs, SeedType seed) {
    auto pipeline = std::make_unique<TerrainPipeline3d>();

    for (const Generator3dSpec& spec : specs) {
        pipeline->push_back(
            makePipelineGenerator3d(spec, seed),
            multiplyFunction(spec.scale)
        );
    }

    pipeline->setSeed(seed);
    return pipeline;
}

} // namespace wgen
