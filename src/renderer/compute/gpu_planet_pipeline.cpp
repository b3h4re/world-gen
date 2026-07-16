#include "gpu_planet_pipeline.hpp"

#include <limits>
#include <stdexcept>

namespace lve {

GpuPlanetPipeline::GpuPlanetPipeline() : accumulator_{device_} {}

namespace {

std::size_t cubeSphereBufferHeight(std::size_t resolution) {
    if (resolution > std::numeric_limits<std::size_t>::max() / wgen::FACES.size()) {
        throw std::overflow_error("GPU cube sphere buffer height overflows size_t");
    }
    return resolution * wgen::FACES.size();
}

} // namespace

wgen::CubeSphere<float> GpuPlanetPipeline::generateCubeSphere(
        const std::vector<GpuPlanetGeneratorRequest>& requests,
        std::size_t resolution) {
    const std::size_t bufferHeight = cubeSphereBufferHeight(resolution);
    GpuHeightMap accumulated{device_, resolution, bufferHeight};
    accumulated.clear();

    GpuHeightMap generated{device_, resolution, bufferHeight};
    for (const GpuPlanetGeneratorRequest& request : requests) {
        generatorFor(request.spec.kind).dispatch(generated, request.spec, request.seed);
        accumulator_.accumulate(
            generated,
            accumulated,
            request.spec.scale * wgen::generator3dOctaveAmplitude(request.spec),
            request.spec.bias
        );
    }

    return wgen::CubeSphere<float>{accumulated.copyToCpu()};
}

GpuPlanetGenerator& GpuPlanetPipeline::generatorFor(wgen::Generator3dKind kind) {
    switch (kind) {
        case wgen::Generator3dKind::PerlinNoise:
            if (perlinNoiseGenerator_ == nullptr) {
                perlinNoiseGenerator_ = makeGpuPlanetGenerator(device_, kind);
            }
            return *perlinNoiseGenerator_;
    }

    throw std::invalid_argument("unknown GPU planet generator kind");
}

} // namespace lve
