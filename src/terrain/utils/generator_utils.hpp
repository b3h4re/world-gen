#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>

namespace wgen {

using HeightFunc = std::function<float(float)>;
using SeedType = std::uint64_t;

float minkowskiDistance(glm::vec2 v1, glm::vec2 v2, float p);

std::size_t wrapIndex(std::size_t index, std::size_t size);

bool isInside(glm::ivec2 pos, glm::ivec2 dir, std::size_t width, std::size_t height);
bool isInsideRectangle(glm::ivec2 pos, glm::ivec2 dir, glm::ivec2 c1, glm::ivec2 c2);

float defaultPerlinInterp(float t);

float lerp(float a, float b, float c);

float defaultReconstructionKernel(float t);

inline auto getLowPassFilter(float a, float b, float c) {
    return [a, b, c](int x) {
        switch (x) {
            case -1:
                return a;
            case 0:
                return b;
            case 1:
                return c;
        }
        throw std::invalid_argument("Filter can only be used for x in {-1, 0, 1}");
    };
}

inline auto defaultDLAHeightFunction(float scale) {
    return [scale](int value) {
        return 1 - 1.0F / (1.0F + scale * static_cast<float>(value));
    };
}

inline auto multiplyFunction(float scale) {
    return [scale](float value) {
        return value * scale;
    };
}

enum class GeneratorDeviceKind {
    Cpu,
    VulkanCompute,
};

struct GeneratorCapabilities {
    bool cpu{true};
    bool vulkanCompute{false};
    bool octaves{false};

    bool supports(GeneratorDeviceKind kind) const {
        switch (kind) {
            case GeneratorDeviceKind::Cpu:
                return cpu;
            case GeneratorDeviceKind::VulkanCompute:
                return vulkanCompute;
        }

        return false;
    }

    bool supportsOctaves() const { return octaves; }
};

} // namespace wgen
