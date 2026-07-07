#pragma once

#include "generator.hpp"
#include "generator_spec.hpp"
#include "terrain_pipeline.hpp"

#include <memory>

namespace wgen {

std::unique_ptr<Generator3d> makeGenerator3d(const Generator3dSpec& spec, SeedType seed);
std::unique_ptr<Generator3d> makePipelineGenerator3d(const Generator3dSpec& spec, SeedType seed);
std::unique_ptr<TerrainPipeline3d> makePipeline3d(const Generator3dPipelineSpec& specs, SeedType seed);

} // namespace wgen
