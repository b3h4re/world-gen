#include "generator_compute_capabilities.hpp"

namespace wgen {

bool generatorSupportsComputeMethod(GeneratorKind kind, TerrainComputeMethod computeMethod) {
    if (computeMethod == TerrainComputeMethod::Cpu) {
        return true;
    }

    return computeMethod == TerrainComputeMethod::VulkanCompute && generatorSupportsVulkanCompute(kind);
}

} // namespace wgen
