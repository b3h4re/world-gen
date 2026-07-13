#include "gpu_planet_generator.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace lve {

namespace {

std::uint32_t checkedComputeDimension3d(std::size_t value, const char* name) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error(std::string{name} + " overflows uint32_t");
    }

    return static_cast<std::uint32_t>(value);
}

} // namespace

PerlinNoise3dGpuGenerator::PerlinNoise3dGpuGenerator(LveComputeDevice& device)
    : computer_{device, "perlin_3d", sizeof(wgen::PerlinNoiseComputeSpec3D)} {}

void PerlinNoise3dGpuGenerator::dispatch(
        GpuHeightMap& output,
        const wgen::Generator3dSpec& spec,
        wgen::SeedType seed) {
    const auto* perlinSpec = std::get_if<wgen::PerlinNoise3dGeneratorSpec>(&spec.config);
    if (spec.kind != wgen::Generator3dKind::PerlinNoise || perlinSpec == nullptr) {
        throw std::invalid_argument("3D perlin GPU generator received wrong spec");
    }
    if (output.height() % wgen::FACES.size() != 0 ||
            output.height() / wgen::FACES.size() != output.width()) {
        throw std::invalid_argument("3D perlin GPU generator requires an N x 6N cube sphere buffer");
    }

    const wgen::PerlinNoiseComputeSpec3D computeSpec{
        .cellSize = perlinSpec->cellSize,
        .coordinateScale = wgen::generator3dOctaveFrequency(spec),
        .faceResolution = checkedComputeDimension3d(output.width(), "GPU cube sphere face resolution"),
        .seed = seed,
    };

    computer_.dispatch(
        computeSpec,
        output.descriptorInfo(),
        ComputeDispatchSize{
            .groupCountX = checkedComputeDimension3d(output.width(), "GPU cube sphere width"),
            .groupCountY = checkedComputeDimension3d(output.width(), "GPU cube sphere face height"),
            .groupCountZ = checkedComputeDimension3d(wgen::FACES.size(), "GPU cube sphere face count"),
        });
}

std::unique_ptr<GpuPlanetGenerator> makeGpuPlanetGenerator(LveComputeDevice& device, wgen::Generator3dKind kind) {
    switch (kind) {
        case wgen::Generator3dKind::PerlinNoise:
            return std::make_unique<PerlinNoise3dGpuGenerator>(device);
    }

    throw std::invalid_argument("unknown GPU planet generator kind");
}

} // namespace lve
