#pragma once

#include "terrain/generators/2d/generator.hpp"
#include "terrain/generators/2d/generator_spec.hpp"
#include "terrain/generators/2d/terrain_pipeline.hpp"

#include <memory>

namespace wgen {

std::unique_ptr<Generator> makeGenerator(const GeneratorSpec& spec, SeedType seed);
std::unique_ptr<Generator> makePipelineGenerator(const GeneratorSpec& spec, SeedType seed);
std::unique_ptr<TerrainPipeline> makePipeline(const GeneratorPipelineSpec& specs, SeedType seed);

} // namespace wgen
