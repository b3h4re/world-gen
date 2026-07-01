#include "generator_factory.hpp"

#include "terrain/generators/noise/perlin.hpp"
#include "terrain/generators/noise/value_noise.hpp"

#include <stdexcept>

namespace wgen {

std::unique_ptr<Generator> makeGenerator(const GeneratorSpec& spec, SeedType seed) {
    switch (spec.kind) {
        case GeneratorKind::ValueNoise:
            if (!std::holds_alternative<ValueNoiseGeneratorSpec>(spec.config)) {
                throw std::invalid_argument("value noise generator spec has wrong config type");
            }

            return std::make_unique<ValueNoiseGenerator>(seed);
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
    }

    throw std::invalid_argument("unknown generator kind");
}

std::unique_ptr<TerrainPipeline> makePipeline(const GeneratorPipelineSpec& specs, SeedType seed) {
    auto pipeline = std::make_unique<TerrainPipeline>();

    for (const GeneratorSpec& spec : specs) {
        pipeline->push_back(makeGenerator(spec, seed), multiplyFunction(spec.scale));
    }

    pipeline->setSeed(seed);
    return pipeline;
}

} // namespace wgen
