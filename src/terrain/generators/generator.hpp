#pragma once

#include "terrain/terrain.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>


namespace wgen {

    using HeightFunc = std::function<float(float)>; // Function whic receives int of point and returns height

    float minkowskiDistance(glm::vec2 v1, glm::vec2 v2, float p);


    std::size_t wrapIndex(std::size_t index, std::size_t size);

    bool isInside(const glm::ivec2 pos, const glm::ivec2 dir, const std::size_t width, const std::size_t height);
    bool isInsideRectangle(const glm::ivec2 pos, const glm::ivec2 dir, glm::ivec2 c1, glm::ivec2 c2);

    float defaultPerlinInterp(float t);

    float lerp(float a, float b, float c);

    float defaultReconstructionKernel(float t);

    // Low pass filter for {-1, 0, 1}, h[a, b, c]
    template<float a, float b, float c>
    float lowPassFilter(int x) {
        switch (x) {
            case -1:
                return a;
                break;
            case 0:
                return b;
                break;
            case 1:
                return c;
                break;
        }
        throw std::invalid_argument("Filter can only be used for x in {-1, 0, 1}");
    }

    // template<float k>
    // float defaultDLAHeightFunction(int x) {
    //     return 1 - 1.0F / (1.0F + k*static_cast<float>(x));
    // }

    inline auto defaultDLAHeightFunction(float scale) {
        return [scale](int value) {
            return 1 - 1.0F / (1.0F + scale*static_cast<float>(value));
        };
    }

    inline auto multiplyFunction(float scale) {
        return [scale](float value) {
            return value*scale;
        };
    }

    enum class GeneratorDeviceKind {
        Cpu,
        VulkanCompute,
    };

    struct GeneratorCapabilities {
        bool cpu{true};
        bool vulkanCompute{false};

        bool supports(GeneratorDeviceKind kind) const {
            switch (kind) {
                case GeneratorDeviceKind::Cpu:
                    return cpu;
                case GeneratorDeviceKind::VulkanCompute:
                    return vulkanCompute;
            }

            return false;
        }
    };

    enum class GeneratorType {
        ValueNoise,
    };

    struct ValueNoiseGeneratorSpec {
        std::uint32_t seed{};
    };

    struct GeneratorSpec {
        GeneratorType type{};
        ValueNoiseGeneratorSpec valueNoise{};
    };


    class Generator {
    public:
        virtual ~Generator() = default;

        virtual HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) const {
            HeightMap<float> map{width, height};
            for (std::size_t y = 0; y < height; ++y) {
                for (std::size_t x = 0; x < width; ++x) {
                    map.at(x, y) = noise(x, y);
                }
            }

            return map;
        }

        virtual GeneratorCapabilities capabilities() const { return {}; }
        virtual GeneratorSpec spec() const {
            throw std::runtime_error("generator does not provide a serializable spec");
        }


        virtual void setSeed(std::uint32_t newSeed) { seed_ = newSeed; }
        std::uint32_t getSeed() const { return seed_; }
        virtual float noise(std::size_t x, std::size_t y) const = 0;

    private:
        std::uint32_t seed_{};
    };

}
