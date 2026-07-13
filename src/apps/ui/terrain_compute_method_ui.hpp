#pragma once

#include "terrain/terrain_compute_method.hpp"

namespace lve {

int computeMethodIndex(wgen::TerrainComputeMethod computeMethod);
wgen::TerrainComputeMethod computeMethodFromIndex(int index);

} // namespace lve
