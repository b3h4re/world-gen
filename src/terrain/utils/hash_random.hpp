#pragma once

#include "terrain/generators/2d/generator.hpp"

#include <utility>
#include <cstdint>

#include <glm/glm.hpp>

namespace wgen {

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

    constexpr std::uint64_t splitmix64(std::uint64_t x) noexcept {
        x += 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }
    SeedType hashSeed(SeedType seed);

    glm::vec2 randomHashDir(std::size_t x, std::size_t y, SeedType seed);

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

    template <typename... Ints>
    std::uint64_t hashValues(SeedType seed, Ints... values) noexcept {
        static_assert((std::is_integral_v<Ints> && ...));

        std::uint64_t h = splitmix64(seed);

        ((h = splitmix64(
            h ^ splitmix64(static_cast<std::uint64_t>(values))
        )), ...);

        return h;
    }

    template <typename Float>
    class HashUniformRealDistribution {
        static_assert(std::is_floating_point_v<Float>);

    public:
        using result_type = Float;

        HashUniformRealDistribution(Float min = Float{0}, Float max = Float{1}) noexcept
            : min_{min}, max_{max} {}

        Float min() const noexcept {
            return min_;
        }

        Float max() const noexcept {
            return max_;
        }

        Float operator()(std::uint64_t hash) const noexcept {
            return min_ + (max_ - min_) * unit(hash);
        }

    private:
        Float min_;
        Float max_;

        static Float unit(std::uint64_t hash) noexcept {
            constexpr int digits = std::numeric_limits<Float>::digits;
            static_assert(digits <= 64);

            // Use the highest `digits` bits.
            const std::uint64_t value = hash >> (64 - digits);

            // value / 2^digits gives [0, 1)
            return static_cast<Float>(value) * std::ldexp(Float{1}, -digits);
        }
    };

    int poisson(float lambda, SeedType seed);

}
