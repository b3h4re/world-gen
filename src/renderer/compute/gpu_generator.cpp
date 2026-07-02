#include "gpu_generator.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace lve {

std::uint32_t checkedComputeDimension(std::size_t value, const char* name) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error(std::string{name} + " overflows uint32_t");
    }

    return static_cast<std::uint32_t>(value);
}

ValueNoiseGpuGenerator::ValueNoiseGpuGenerator(LveComputeDevice& device)
    : computer_{device, "value_noise", sizeof(wgen::ValueNoiseComputeSpec)} {}

void ValueNoiseGpuGenerator::dispatch(
        GpuHeightMap& output,
        const wgen::GeneratorSpec& spec,
        wgen::SeedType seed) {
    if (spec.kind != wgen::GeneratorKind::ValueNoise ||
            !std::holds_alternative<wgen::ValueNoiseGeneratorSpec>(spec.config)) {
        throw std::invalid_argument("value noise GPU generator received wrong spec");
    }

    wgen::ValueNoiseComputeSpec computeSpec{
        .width = checkedComputeDimension(output.width(), "GPU heightmap width"),
        .height = checkedComputeDimension(output.height(), "GPU heightmap height"),
        .seed = seed,
    };

    computer_.dispatch(
        computeSpec,
        output.descriptorInfo(),
        ComputeDispatchSize{
            .groupCountX = Computer::dispatchGroupCount(output.width(), 16),
            .groupCountY = Computer::dispatchGroupCount(output.height(), 16),
            .groupCountZ = 1,
        });
}

PerlinNoiseGpuGenerator::PerlinNoiseGpuGenerator(LveComputeDevice& device)
    : computer_{device, "perlin_noise", sizeof(wgen::PerlinNoiseComputeSpec)} {}

void PerlinNoiseGpuGenerator::dispatch(
        GpuHeightMap& output,
        const wgen::GeneratorSpec& spec,
        wgen::SeedType seed) {
    const auto* perlinSpec = std::get_if<wgen::PerlinNoiseGeneratorSpec>(&spec.config);
    if (spec.kind != wgen::GeneratorKind::PerlinNoise || perlinSpec == nullptr) {
        throw std::invalid_argument("perlin GPU generator received wrong spec");
    }

    wgen::PerlinNoiseComputeSpec computeSpec{
        .dots = checkedComputeDimension(perlinSpec->dotsPerCell, "perlin dots per cell"),
        .seed = seed,
    };

    computer_.dispatch(
        computeSpec,
        output.descriptorInfo(),
        ComputeDispatchSize{
            .groupCountX = checkedComputeDimension(output.width(), "GPU heightmap width"),
            .groupCountY = checkedComputeDimension(output.height(), "GPU heightmap height"),
            .groupCountZ = 1,
        });
}

std::unique_ptr<GpuGenerator> makeGpuGenerator(LveComputeDevice& device, wgen::GeneratorKind kind) {
    switch (kind) {
        case wgen::GeneratorKind::ValueNoise:
            return std::make_unique<ValueNoiseGpuGenerator>(device);
        case wgen::GeneratorKind::PerlinNoise:
            return std::make_unique<PerlinNoiseGpuGenerator>(device);
    }

    throw std::invalid_argument("unknown GPU generator kind");
}

} // namespace lve
