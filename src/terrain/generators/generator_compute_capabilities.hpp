#pragma once

#include "terrain/generators/generator_spec.hpp"
#include "terrain/terrain_compute_method.hpp"

namespace wgen {

bool generatorSupportsComputeMethod(GeneratorKind kind, TerrainComputeMethod computeMethod);

} // namespace wgen
