#pragma once

#include "device/lve_compute_device.hpp"
#include "renderer/compute/computer.hpp"
#include "renderer/compute/gpu_height_map.hpp"
#include "terrain/generators/generator_spec.hpp"
#include "terrain/generators/generator.hpp"

#include <array>
#include <cstddef>
#include <memory>

namespace lve {

class GpuGenerator {
public:
    virtual ~GpuGenerator() = default;

    virtual void dispatch(
        GpuHeightMap& output,
        const wgen::GeneratorSpec& spec,
        wgen::SeedType seed) = 0;
};

class ValueNoiseGpuGenerator final : public GpuGenerator {
public:
    explicit ValueNoiseGpuGenerator(LveComputeDevice& device);

    void dispatch(
        GpuHeightMap& output,
        const wgen::GeneratorSpec& spec,
        wgen::SeedType seed) override;

private:
    Computer computer_;
};

class PerlinNoiseGpuGenerator final : public GpuGenerator {
public:
    explicit PerlinNoiseGpuGenerator(LveComputeDevice& device);

    void dispatch(
        GpuHeightMap& output,
        const wgen::GeneratorSpec& spec,
        wgen::SeedType seed) override;

private:
    Computer computer_;
};

class WorleyNoiseGpuGenerator final : public GpuGenerator {
public:
    explicit WorleyNoiseGpuGenerator(LveComputeDevice& device);

    void dispatch(
        GpuHeightMap& output,
        const wgen::GeneratorSpec& spec,
        wgen::SeedType seed) override;

private:
    Computer& computerForFeaturePointCount(std::size_t numPoints);

    LveComputeDevice& device_;
    std::array<std::unique_ptr<Computer>, 8> computers_{};
};

class SimplexNoiseGpuGenerator final : public GpuGenerator {
public:
    explicit SimplexNoiseGpuGenerator(LveComputeDevice& device);

    void dispatch(
        GpuHeightMap& output,
        const wgen::GeneratorSpec& spec,
        wgen::SeedType seed) override;

private:
    Computer computer_;
};

class WaveletNoiseGpuGenerator final : public GpuGenerator {
public:
    explicit WaveletNoiseGpuGenerator(LveComputeDevice& device);

    void dispatch(
        GpuHeightMap& output,
        const wgen::GeneratorSpec& spec,
        wgen::SeedType seed) override;

private:
    Computer computer_;
};


std::unique_ptr<GpuGenerator> makeGpuGenerator(LveComputeDevice& device, wgen::GeneratorKind kind);

} // namespace lve
