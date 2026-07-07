#include "generator_factory.hpp"

#include <stdexcept>

namespace wgen {

std::unique_ptr<Generator3d> makeGenerator3d(const Generator3dSpec&, SeedType) {
    throw std::invalid_argument("no concrete 3D terrain generators are implemented yet");
}

std::unique_ptr<Generator3d> makePipelineGenerator3d(const Generator3dSpec& spec, SeedType seed) {
    return makeGenerator3d(spec, seed);
}

std::unique_ptr<TerrainPipeline3d> makePipeline3d(const Generator3dPipelineSpec& specs, SeedType seed) {
    auto pipeline = std::make_unique<TerrainPipeline3d>();

    for (const Generator3dSpec& spec : specs) {
        pipeline->push_back(
            makePipelineGenerator3d(spec, seed),
            multiplyFunction(spec.scale)
        );
    }

    pipeline->setSeed(seed);
    return pipeline;
}

} // namespace wgen
