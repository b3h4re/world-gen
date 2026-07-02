#include "gpu_terrain_pipeline.hpp"

#include <stdexcept>

namespace lve {

GpuTerrainPipeline::GpuTerrainPipeline() : accumulator_{device_} {}

wgen::HeightMap<float> GpuTerrainPipeline::generateHeightMap(
        const std::vector<GpuGeneratorRequest>& requests,
        std::size_t width,
        std::size_t height) {
    GpuHeightMap accumulated{device_, width, height};
    accumulated.clear();

    GpuHeightMap generated{device_, width, height};
    for (const GpuGeneratorRequest& request : requests) {
        generatorFor(request.spec.kind).dispatch(generated, request.spec, request.seed);
        accumulator_.accumulate(generated, accumulated, request.spec.scale);
    }

    return accumulated.copyToCpu();
}

GpuGenerator& GpuTerrainPipeline::generatorFor(wgen::GeneratorKind kind) {
    switch (kind) {
        case wgen::GeneratorKind::ValueNoise:
            if (valueNoiseGenerator_ == nullptr) {
                valueNoiseGenerator_ = makeGpuGenerator(device_, kind);
            }
            return *valueNoiseGenerator_;
        case wgen::GeneratorKind::PerlinNoise:
            if (perlinNoiseGenerator_ == nullptr) {
                perlinNoiseGenerator_ = makeGpuGenerator(device_, kind);
            }
            return *perlinNoiseGenerator_;
        case wgen::GeneratorKind::WorleyNoise:
            if (worleyNoiseGenerator_ == nullptr) {
                worleyNoiseGenerator_ = makeGpuGenerator(device_, kind);
            }
            return *worleyNoiseGenerator_;
    }

    throw std::invalid_argument("unknown GPU generator kind");
}

} // namespace lve
