#pragma once

#include "terrain/terrain.hpp"

#include <cstdint>
#include <utility>
#include <cmath>


namespace wgen {

    float minkowskiDistance(glm::vec2 v1, glm::vec2 v2, float p);

    constexpr std::uint64_t splitmix64(std::uint64_t x) noexcept {
        x += 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }

    constexpr std::uint64_t makeKey(int i, int j) noexcept {
        auto ui = static_cast<std::uint32_t>(i);
        auto uj = static_cast<std::uint32_t>(j);

        return (static_cast<std::uint64_t>(ui) << 32)
            |  static_cast<std::uint64_t>(uj);
    }

    constexpr float toUnitFloat(std::uint64_t h) noexcept {
        // Uses 24 random bits, matching float mantissa precision.
        constexpr float scale = 1.0f / static_cast<float>(1u << 24);

        return static_cast<float>((h >> 40) & 0xFFFFFFu) * scale;
    }

    constexpr float toSignedFloat(std::uint64_t h) noexcept {
        // [0, 1) -> [-1, 1)
        return 2.0f * toUnitFloat(h) - 1.0f;
    }

    constexpr glm::vec2 hash2(int i, int j) noexcept {
        std::uint64_t key = makeKey(i, j);

        std::uint64_t h1 = splitmix64(key);
        std::uint64_t h2 = splitmix64(h1);

        return {toSignedFloat(h1), toSignedFloat(h2)};
    }

    std::size_t wrapIndex(std::size_t index, std::size_t size);

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

    class Generator {
    public:
        virtual ~Generator() = default;

        HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) const {
            HeightMap<float> map{width, height};
            for (std::size_t y = 0; y < height; ++y) {
                for (std::size_t x = 0; x < width; ++x) {
                    map.at(x, y) = noise(x, y);
                }
            }

            return map;
        }

        virtual float noise(std::size_t x, std::size_t y) const = 0;

        virtual void setSeed(std::uint32_t newSeed) { seed_ = newSeed; }
        std::uint32_t getSeed() const { return seed_; }

    private:
        std::uint32_t seed_{};
    };

}
