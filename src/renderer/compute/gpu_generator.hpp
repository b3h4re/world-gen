#pragma once

#include "device/lve_compute_device.hpp"
#include "renderer/compute/computer.hpp"
#include "renderer/compute/gpu_height_map.hpp"
#include "terrain/generators/generator_spec.hpp"
#include "terrain/generators/generator.hpp"

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

std::unique_ptr<GpuGenerator> makeGpuGenerator(LveComputeDevice& device, wgen::GeneratorKind kind);

} // namespace lve
