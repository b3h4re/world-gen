#pragma once

#include "terrain/terrain.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>


namespace wgen {

    using HeightFunc = std::function<float(float)>; // Function whic receives int of point and returns height

    struct Ivec2Hash {
        std::size_t operator()(const glm::ivec2& v) const noexcept {
            std::size_t h1 = std::hash<int>{}(v.x);
            std::size_t h2 = std::hash<int>{}(v.y);

            // hash combine
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
    struct Vec2Hash {
        std::size_t operator()(const glm::vec2& v) const noexcept {
            std::size_t h1 = std::hash<int>{}(v.x);
            std::size_t h2 = std::hash<int>{}(v.y);

            // hash combine
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
    struct Vec2Vec2Hash {
        std::size_t operator()(const std::pair<glm::ivec2, glm::ivec2>& p) const {
            auto hashVec2 = [](const glm::ivec2& v) {
                std::size_t h1 = std::hash<int>{}(v.x);
                std::size_t h2 = std::hash<int>{}(v.y);

                return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
            };

            std::size_t h1 = hashVec2(p.first);
            std::size_t h2 = hashVec2(p.second);

            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
    struct SizeSizeHash {
        std::size_t operator()(const std::pair<std::size_t, std::size_t>& p) const {
            std::size_t h1 = std::hash<int>{}(p.first);
            std::size_t h2 = std::hash<int>{}(p.second);

            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    float minkowskiDistance(glm::vec2 v1, glm::vec2 v2, float p);

    constexpr std::uint64_t splitmix64(std::uint64_t x) noexcept {
        x += 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }
    std::uint32_t hashSeed(std::uint32_t seed);

    glm::vec2 randomHashDir(std::size_t x, std::size_t y, std::uint32_t seed);

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


        virtual void setSeed(std::uint32_t newSeed) { seed_ = newSeed; }
        std::uint32_t getSeed() const { return seed_; }
        virtual float noise(std::size_t x, std::size_t y) const = 0;

    private:
        std::uint32_t seed_{};
    };

}
