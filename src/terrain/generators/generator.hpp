#pragma once

#include "terrain/terrain.hpp"
#include "renderer/compute/computer.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>


namespace wgen {

    using HeightFunc = std::function<float(float)>; // Function whic receives int of point and returns height
    using SeedType = std::uint64_t;

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

    struct ValueNoiseComputeSpec {
        std::uint32_t width{};
        std::uint32_t height{};
        float coordinateScale{1.0F};
        wgen::SeedType seed{};
    };
    static_assert(offsetof(ValueNoiseComputeSpec, width) == 0);
    static_assert(offsetof(ValueNoiseComputeSpec, height) == 4);
    static_assert(offsetof(ValueNoiseComputeSpec, coordinateScale) == 8);
    static_assert(offsetof(ValueNoiseComputeSpec, seed) == 16);

    struct PerlinNoiseComputeSpec {
        std::uint32_t dots{};
        float coordinateScale{1.0F};
        SeedType seed{};
    };
    static_assert(offsetof(PerlinNoiseComputeSpec, dots) == 0);
    static_assert(offsetof(PerlinNoiseComputeSpec, coordinateScale) == 4);
    static_assert(offsetof(PerlinNoiseComputeSpec, seed) == 8);

    struct GaborNoiseComputeSpec {
        std::uint32_t dots{};
        alignas(8) SeedType seed{};
        // {impulseDensity, kernelSpatialExtent, kernelOscillationFrequency, coordinateScale}
        alignas(16) glm::vec4 gaborParams{4.0f, 1.5f, 1.0f, 1.0f};
    };
    static_assert(offsetof(GaborNoiseComputeSpec, dots) == 0);
    static_assert(offsetof(GaborNoiseComputeSpec, seed) == 8);
    static_assert(offsetof(GaborNoiseComputeSpec, gaborParams) == 16);
    static_assert(sizeof(GaborNoiseComputeSpec) == 32);

    struct WorleyNoiseComputeSpec {
        alignas(4) std::uint32_t dots{};
        alignas(4) float p{2.0F};
        alignas(4) float coordinateScale{1.0F};
        alignas(4) std::uint32_t numPoints{1};
        alignas(8) SeedType seed{};
    };
    static_assert(offsetof(WorleyNoiseComputeSpec, dots) == 0);
    static_assert(offsetof(WorleyNoiseComputeSpec, p) == 4);
    static_assert(offsetof(WorleyNoiseComputeSpec, coordinateScale) == 8);
    static_assert(offsetof(WorleyNoiseComputeSpec, numPoints) == 12);
    static_assert(offsetof(WorleyNoiseComputeSpec, seed) == 16);

    struct SimplexNoiseComputeSpec {
        std::uint32_t dots{};
        float coordinateScale{1.0F};
        SeedType seed{};
    };
    static_assert(offsetof(SimplexNoiseComputeSpec, dots) == 0);
    static_assert(offsetof(SimplexNoiseComputeSpec, coordinateScale) == 4);
    static_assert(offsetof(SimplexNoiseComputeSpec, seed) == 8);

    struct alignas(16) WaveletNoiseComputeSpec {
        glm::vec4 waveletParams{}; // A, B, C, frequency
        std::uint32_t kWidth{};
        std::uint32_t kHeight{};
        SeedType seed{};
        float coordinateScale{1.0F};
    };
    static_assert(offsetof(WaveletNoiseComputeSpec, waveletParams) == 0);
    static_assert(offsetof(WaveletNoiseComputeSpec, kWidth) == 16);
    static_assert(offsetof(WaveletNoiseComputeSpec, kHeight) == 20);
    static_assert(offsetof(WaveletNoiseComputeSpec, seed) == 24);
    static_assert(offsetof(WaveletNoiseComputeSpec, coordinateScale) == 32);


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
        virtual std::string compShader() const {
            throw std::runtime_error("generator does not have a .comp shader");
        }
        virtual std::size_t specSize() const {
            throw std::runtime_error("generator does not provide a spec");
        }


        virtual void setSeed(SeedType newSeed) { seed_ = newSeed; }
        SeedType getSeed() const { return seed_; }
        virtual float noise(std::size_t x, std::size_t y) const = 0;

    private:
        SeedType seed_{};
    };

    template <typename GenClass>
    concept has_noise_function = requires (const GenClass& gen, std::size_t x, std::size_t y) {
        { gen.noise(x, y) } -> std::convertible_to<float>;
    };


    template <typename GenClass>
    concept valid_generator =
        std::derived_from<GenClass, Generator> &&
        has_noise_function<GenClass> &&
        (
            std::constructible_from<GenClass, SeedType> ||
            std::constructible_from<GenClass, std::size_t, SeedType> ||
            std::constructible_from<GenClass, std::size_t, std::size_t, SeedType> ||
            std::constructible_from<GenClass, std::size_t, std::size_t, std::size_t, SeedType> ||
            std::constructible_from<GenClass, SeedType, glm::vec<2, std::size_t>, glm::vec4> ||
            std::constructible_from<GenClass, glm::vec<2, std::size_t>, glm::vec4> ||
            std::constructible_from<GenClass, std::size_t, SeedType, float, float, float>
        );

}
