#include "gpu_planet_pipeline.hpp"

#include <stdexcept>

namespace lve {

GpuPlanetPipeline::GpuPlanetPipeline() : accumulator_{device_} {}

wgen::Planet<float> GpuPlanetPipeline::generatePlanet(
        const std::vector<GpuPlanetGeneratorRequest>& requests,
        std::size_t dots) {
    GpuHeightMap accumulated{device_, dots, dots};
    accumulated.clear();

    GpuHeightMap generated{device_, dots, dots};
    for (const GpuPlanetGeneratorRequest& request : requests) {
        generatorFor(request.spec.kind).dispatch(generated, request.spec, request.seed);
        accumulator_.accumulate(
            generated,
            accumulated,
            request.spec.scale * wgen::generator3dOctaveAmplitude(request.spec)
        );
    }

    return wgen::Planet<float>{accumulated.copyToCpu()};
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
