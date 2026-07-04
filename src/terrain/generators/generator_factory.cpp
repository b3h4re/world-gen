#include "generator_factory.hpp"

#include "terrain/generators/coordinate_scaled_generator.hpp"
#include "terrain/generators/noise/worley.hpp"
#include "terrain/generators/noise/simplex.hpp"
#include "terrain/generators/noise/perlin.hpp"
#include "terrain/generators/noise/gabor.hpp"
#include "terrain/generators/noise/value_noise.hpp"
#include "terrain/generators/noise/wavelet.hpp"

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
        case GeneratorKind::GaborNoise: {
            const auto* config = std::get_if<GaborNoiseGeneratorSpec>(&spec.config);
            if (config == nullptr) {
                throw std::invalid_argument("gabor noise generator spec has wrong config type");
            }

            return std::make_unique<GaborNoise>(
                config->dotsPerCell,
                seed,
                config->impulseDensity,
                config->kernelSpatialExtent,
                config->kernelOscillationFrequency,
                config->oscillationOrientation
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
        case GeneratorKind::WaveletNoise: {
            const auto* config = std::get_if<WaveletNoiseGeneratorSpec>(&spec.config);
            if (config == nullptr) {
                throw std::invalid_argument("wavelet noise generator spec has wrong config type");
            }

            return std::make_unique<WaveletNoise2d>(
                seed,
                glm::vec<2, std::size_t>{config->kWidth, config->kheight},
                config->waveletParams
            );
        }
    }

    throw std::invalid_argument("unknown generator kind");
}

void validateOctaveSettings(const GeneratorOctaveSettings& settings) {
    if (settings.lacunarity <= 0.0F) {
        throw std::invalid_argument("octave lacunarity must be positive");
    }
    if (settings.persistance < 0.0F) {
        throw std::invalid_argument("octave persistance must be non-negative");
    }
}

std::unique_ptr<Generator> makeOctaveGenerator(const GeneratorSpec& spec, SeedType seed) {
    validateOctaveSettings(spec.octaveSettings);
    auto generator = makeGenerator(spec, seed);
    if (!generator->capabilities().supportsOctaves()) {
        throw std::invalid_argument("generator spec requested octaves for a generator that does not support them");
    }

    return std::make_unique<CoordinateScaledGenerator>(
        std::move(generator),
        generatorOctaveFrequency(spec)
    );
}

std::unique_ptr<Generator> makePipelineGenerator(const GeneratorSpec& spec, SeedType seed) {
    auto generator = makeGenerator(spec, seed);
    if (generator->capabilities().supportsOctaves()) {
        generator = makeOctaveGenerator(spec, seed);
    }

    return generator;
}

std::unique_ptr<TerrainPipeline> makePipeline(const GeneratorPipelineSpec& specs, SeedType seed) {
    auto pipeline = std::make_unique<TerrainPipeline>();

    for (const GeneratorSpec& spec : specs) {
        pipeline->push_back(
            makePipelineGenerator(spec, seed),
            multiplyFunction(spec.scale * generatorOctaveAmplitude(spec))
        );
    }

    pipeline->setSeed(seed);
    return pipeline;
}

} // namespace wgen
