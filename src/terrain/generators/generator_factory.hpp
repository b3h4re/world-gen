#pragma once

#include "terrain/generators/generator.hpp"
#include "terrain/generators/generator_spec.hpp"
#include "terrain/generators/terrain_pipeline.hpp"

#include <memory>

namespace wgen {

std::unique_ptr<Generator> makeGenerator(const GeneratorSpec& spec, SeedType seed);
std::unique_ptr<TerrainPipeline> makePipeline(const GeneratorPipelineSpec& specs, SeedType seed);

} // namespace wgen
