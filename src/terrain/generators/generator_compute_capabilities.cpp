#include "generator_compute_capabilities.hpp"

namespace wgen {

bool generatorSupportsComputeMethod(GeneratorKind kind, TerrainComputeMethod computeMethod) {
    if (computeMethod == TerrainComputeMethod::Cpu) {
        return true;
    }

    switch (kind) {
        case GeneratorKind::PerlinNoise:
        case GeneratorKind::ValueNoise:
            return computeMethod == TerrainComputeMethod::VulkanCompute;
    }

    return false;
}

} // namespace wgen
