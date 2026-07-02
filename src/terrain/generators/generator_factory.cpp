#include "generator_factory.hpp"

#include "terrain/generators/coordinate_scaled_generator.hpp"
#include "terrain/generators/noise/worley.hpp"
#include "terrain/generators/noise/simplex.hpp"
#include "terrain/generators/noise/perlin.hpp"
#include "terrain/generators/noise/value_noise.hpp"

#include <cmath>
#include <stdexcept>

namespace wgen {

std::unique_ptr<Generator> makeGenerator(const GeneratorSpec& spec, SeedType seed) {
    switch (spec.kind) {
        case GeneratorKind::ValueNoise: {
            if (!std::holds_alternative<ValueNoiseGeneratorSpec>(spec.config)) {
                throw std::invalid_argument("value noise generator spec has wrong config type");
            }

            return std::make_unique<ValueNoiseGenerator>(seed);
        }
        case GeneratorKind::PerlinNoise: {
            const auto* config = std::get_if<PerlinNoiseGeneratorSpec>(&spec.config);
            if (config == nullptr) {
                throw std::invalid_argument("perlin noise generator spec has wrong config type");
            }

            return std::make_unique<PerlinNoise2d>(
                config->dotsPerCell,
                seed
            );
        }
        case GeneratorKind::WorleyNoise: {
            const auto* config = std::get_if<WorleyNoiseGeneratorSpec>(&spec.config);
            if (config == nullptr) {
                throw std::invalid_argument("worley noise generator spec has wrong config type");
            }

            return std::make_unique<WorleyNoise2d>(
                config->dotsPerCell,
                seed,
                config->p,
                config->numPoints
            );
        }
        case GeneratorKind::SimplexNoise: {
            const auto* config = std::get_if<SimplexNoiseGeneratorSpec>(&spec.config);
            if (config == nullptr) {
                throw std::invalid_argument("simplex noise generator spec has wrong config type");
            }

            return std::make_unique<SimplexNoise2d>(
                config->dotsPerCell,
                seed
            );
        }
    }

    throw std::invalid_argument("unknown generator kind");
}

void validateOctaveSettings(const GeneratorOctaveSettings& settings) {
    if (settings.numOctaves == 0) {
        throw std::invalid_argument("octave count must be at least one");
    }
    if (settings.lacunarity <= 0.0F) {
        throw std::invalid_argument("octave lacunarity must be positive");
    }
    if (settings.persistance < 0.0F) {
        throw std::invalid_argument("octave persistance must be non-negative");
    }
}

std::unique_ptr<TerrainPipeline> makeOctavePipeline(const GeneratorSpec& spec, SeedType seed) {
    validateOctaveSettings(spec.octaveSettings);

    auto pipeline = std::make_unique<TerrainPipeline>();
    for (std::size_t octave = 0; octave < spec.octaveSettings.numOctaves; ++octave) {
        auto generator = makeGenerator(spec, seed);
        if (!generator->capabilities().supportsOctaves()) {
            throw std::invalid_argument("generator spec requested octaves for a generator that does not support them");
        }

        const float frequency = std::pow(spec.octaveSettings.lacunarity, static_cast<float>(octave));
        const float amplitude = std::pow(spec.octaveSettings.persistance, static_cast<float>(octave));
        pipeline->push_back(
            std::make_unique<CoordinateScaledGenerator>(std::move(generator), frequency),
            multiplyFunction(amplitude)
        );
    }

    pipeline->setSeed(seed);
    return pipeline;
}

std::unique_ptr<TerrainPipeline> makePipeline(const GeneratorPipelineSpec& specs, SeedType seed) {
    auto pipeline = std::make_unique<TerrainPipeline>();

    for (const GeneratorSpec& spec : specs) {
        auto generator = makeGenerator(spec, seed);
        if (generator->capabilities().supportsOctaves()) {
            generator = makeOctavePipeline(spec, seed);
        }

        pipeline->push_back(std::move(generator), multiplyFunction(spec.scale));
    }

    pipeline->setSeed(seed);
    return pipeline;
}

} // namespace wgen
