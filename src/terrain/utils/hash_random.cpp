#include "hash_random.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>


namespace wgen {

    SeedType hashSeed(SeedType seed) {
        const std::uint64_t hashed = splitmix64(seed);
        return hashed ^ (hashed >> 32);
    }

    glm::vec2 randomHashDir(std::size_t x, std::size_t y, SeedType seed) {
        std::uint64_t h = seed;
        h ^= splitmix64(static_cast<std::uint64_t>(x) + 0x9E3779B97F4A7C15ull);
        h ^= splitmix64(static_cast<std::uint64_t>(y) + 0xBF58476D1CE4E5B9ull);

        h = splitmix64(h);

        const float u = static_cast<double>(h >> 11) * (1.0 / 9007199254740992.0);
        const float angle = u * 2.0 * std::numbers::pi_v<double>;
        return {
            std::cos(angle),
            std::sin(angle)
        };
    }

}
