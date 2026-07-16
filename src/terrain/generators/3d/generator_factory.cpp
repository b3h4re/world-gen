#include "generator_factory.hpp"

#include "terrain/generators/3d/coordinate_scaled_generator.hpp"
#include "terrain/generators/3d/noise/perlin.hpp"

#include <cmath>
#include <stdexcept>

namespace wgen {

void validateOctaveSettings3d(const GeneratorOctaveSettings& settings) {
    if (settings.lacunarity <= 0.0F) {
        throw std::invalid_argument("3D octave lacunarity must be positive");
    }
    if (settings.persistance < 0.0F) {
        throw std::invalid_argument("3D octave persistance must be non-negative");
    }
}

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
    validateOctaveSettings3d(spec.octaveSettings);
    auto generator = makeGenerator3d(spec, seed);
    if (generator->capabilities().supportsOctaves()) {
        if (!generator3dSupportsOctaves(spec.kind)) {
            throw std::invalid_argument("3D generator spec requested octaves for a generator that does not support them");
        }

        generator = std::make_unique<CoordinateScaledGenerator3d>(
            std::move(generator),
            generator3dOctaveFrequency(spec)
        );
    }

    return generator;
}

std::unique_ptr<TerrainPipeline3d> makePipeline3d(const Generator3dPipelineSpec& specs, SeedType seed) {
    auto pipeline = std::make_unique<TerrainPipeline3d>();

    for (const Generator3dSpec& spec : specs) {
        if (!std::isfinite(spec.scale) || !std::isfinite(spec.bias)) {
            throw std::invalid_argument("3D generator scale and bias must be finite");
        }
        const float scale = spec.scale * generator3dOctaveAmplitude(spec);
        pipeline->push_back(
            makePipelineGenerator3d(spec, seed),
            [scale, bias = spec.bias](float height) {
                return height * scale + bias;
            }
        );
    }

    pipeline->setSeed(seed);
    return pipeline;
}

} // namespace wgen
