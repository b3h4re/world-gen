#include "terrain_compute_method_ui.hpp"

namespace lve {

int computeMethodIndex(wgen::TerrainComputeMethod computeMethod) {
    switch (computeMethod) {
        case wgen::TerrainComputeMethod::Cpu:
            return 0;
        case wgen::TerrainComputeMethod::VulkanCompute:
            return 1;
    }

    return 0;
}

wgen::TerrainComputeMethod computeMethodFromIndex(int index) {
    switch (index) {
        case 1:
            return wgen::TerrainComputeMethod::VulkanCompute;
        case 0:
        default:
            return wgen::TerrainComputeMethod::Cpu;
    }
}

} // namespace lve
