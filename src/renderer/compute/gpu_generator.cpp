#include "gpu_generator.hpp"

#include "terrain/generators/noise/worley.hpp"

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

float checkedPowerDimension(float value, const char* name) {
    if (value > std::numeric_limits<float>::max()) {
        throw std::overflow_error(std::string{name} + " overflows flaot");
    }
    if (std::abs(value) < 0.01F) {
        throw std::invalid_argument(std::string{name} + "power too small");
    }

    return value;
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
        .coordinateScale = wgen::generatorOctaveFrequency(spec),
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
        .coordinateScale = wgen::generatorOctaveFrequency(spec),
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

GaborNoiseGpuGenerator::GaborNoiseGpuGenerator(LveComputeDevice& device)
    : computer_{device, "gabor_noise", sizeof(wgen::GaborNoiseComputeSpec)} {}

void GaborNoiseGpuGenerator::dispatch(
        GpuHeightMap& output,
        const wgen::GeneratorSpec& spec,
        wgen::SeedType seed) {
    const auto* gaborSpec = std::get_if<wgen::GaborNoiseGeneratorSpec>(&spec.config);
    if (spec.kind != wgen::GeneratorKind::GaborNoise || gaborSpec == nullptr) {
        throw std::invalid_argument("gabor GPU generator received wrong spec");
    }

    wgen::GaborNoiseComputeSpec computeSpec{
        .dots = checkedComputeDimension(gaborSpec->dotsPerCell, "gabor dots per cell"),
        .seed = seed,
        .gaborParams = glm::vec4{gaborSpec->impulseDensity, gaborSpec->kernelSpatialExtent, gaborSpec->kernelOscillationFrequency, wgen::generatorOctaveFrequency(spec)}
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

WorleyNoiseGpuGenerator::WorleyNoiseGpuGenerator(LveComputeDevice& device)
    : device_{device} {}

Computer& WorleyNoiseGpuGenerator::computerForFeaturePointCount(std::size_t numPoints) {
    if (numPoints < wgen::WorleyNoise2d::MIN_GPU_FEATURE_POINT_COUNT ||
            numPoints > wgen::WorleyNoise2d::MAX_GPU_FEATURE_POINT_COUNT) {
        throw std::invalid_argument("Worley GPU generator supports 1 to 8 feature points");
    }

    const std::size_t index = numPoints - wgen::WorleyNoise2d::MIN_GPU_FEATURE_POINT_COUNT;
    if (computers_[index] == nullptr) {
        computers_[index] = std::make_unique<Computer>(
            device_,
            "worley_noise_" + std::to_string(numPoints),
            sizeof(wgen::WorleyNoiseComputeSpec));
    }

    return *computers_[index];
}

void WorleyNoiseGpuGenerator::dispatch(
        GpuHeightMap& output,
        const wgen::GeneratorSpec& spec,
        wgen::SeedType seed) {
    const auto* worleySpec = std::get_if<wgen::WorleyNoiseGeneratorSpec>(&spec.config);
    if (spec.kind != wgen::GeneratorKind::WorleyNoise || worleySpec == nullptr) {
        throw std::invalid_argument("worley GPU generator received wrong spec");
    }

    wgen::WorleyNoiseComputeSpec computeSpec{
        .dots = checkedComputeDimension(worleySpec->dotsPerCell, "worley dots per cell"),
        .p = checkedPowerDimension(worleySpec->p, "worley minkowsky distance power"),
        .coordinateScale = wgen::generatorOctaveFrequency(spec),
        .seed = seed,
    };

    computerForFeaturePointCount(worleySpec->numPoints).dispatch(
        computeSpec,
        output.descriptorInfo(),
        ComputeDispatchSize{
            .groupCountX = checkedComputeDimension(output.width(), "GPU heightmap width"),
            .groupCountY = checkedComputeDimension(output.height(), "GPU heightmap height"),
            .groupCountZ = 1,
        });
}

SimplexNoiseGpuGenerator::SimplexNoiseGpuGenerator(LveComputeDevice& device)
    : computer_{device, "simplex_noise", sizeof(wgen::SimplexNoiseComputeSpec)} {}

void SimplexNoiseGpuGenerator::dispatch(
        GpuHeightMap& output,
        const wgen::GeneratorSpec& spec,
        wgen::SeedType seed) {
    const auto* simplexSpec = std::get_if<wgen::SimplexNoiseGeneratorSpec>(&spec.config);
    if (spec.kind != wgen::GeneratorKind::SimplexNoise || simplexSpec == nullptr) {
        throw std::invalid_argument("simplex GPU generator received wrong spec");
    }

    wgen::SimplexNoiseComputeSpec computeSpec{
        .dots = checkedComputeDimension(simplexSpec->dotsPerCell, "simplex dots per cell"),
        .coordinateScale = wgen::generatorOctaveFrequency(spec),
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

WaveletNoiseGpuGenerator::WaveletNoiseGpuGenerator(LveComputeDevice& device)
    : computer_{device, "wavelet_noise", sizeof(wgen::WaveletNoiseComputeSpec)} {}

void WaveletNoiseGpuGenerator::dispatch(
        GpuHeightMap& output,
        const wgen::GeneratorSpec& spec,
        wgen::SeedType seed) {
    const auto* waveletSpec = std::get_if<wgen::WaveletNoiseGeneratorSpec>(&spec.config);
    if (spec.kind != wgen::GeneratorKind::WaveletNoise || waveletSpec == nullptr) {
        throw std::invalid_argument("wavelet GPU generator received wrong spec");
    }

    wgen::WaveletNoiseComputeSpec computeSpec{
        .waveletParams = waveletSpec->waveletParams,
        .kWidth = checkedComputeDimension(waveletSpec->kWidth, "Kernel width for wavelet"),
        .kHeight = checkedComputeDimension(waveletSpec->kheight, "Kernel height for wavelet"),
        .seed = seed,
        .coordinateScale = wgen::generatorOctaveFrequency(spec)
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
        case wgen::GeneratorKind::GaborNoise:
            return std::make_unique<GaborNoiseGpuGenerator>(device);
        case wgen::GeneratorKind::PerlinNoise:
            return std::make_unique<PerlinNoiseGpuGenerator>(device);
        case wgen::GeneratorKind::WorleyNoise:
            return std::make_unique<WorleyNoiseGpuGenerator>(device);
        case wgen::GeneratorKind::SimplexNoise:
            return std::make_unique<SimplexNoiseGpuGenerator>(device);
        case wgen::GeneratorKind::WaveletNoise:
            return std::make_unique<WaveletNoiseGpuGenerator>(device);
    }

    throw std::invalid_argument("unknown GPU generator kind");
}

} // namespace lve
