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

        const float u = static_cast<float>(h >> 11) * (1.0 / 9007199254740992.0);
        const float angle = u * 2.0 * std::numbers::pi_v<float>;
        return {
            std::cos(angle),
            std::sin(angle)
        };
    }

    glm::vec3 randomHashDir3D(int x, int y, int z, SeedType seed) {
        std::uint64_t h = seed;

        h ^= splitmix64(static_cast<std::uint64_t>(x) + 0x9E3779B97F4A7C15ull);
        h ^= splitmix64(static_cast<std::uint64_t>(y) + 0xBF58476D1CE4E5B9ull);
        h ^= splitmix64(static_cast<std::uint64_t>(z) + 0x94D049BB133111EBull);

        h = splitmix64(h);

        const float u1 = static_cast<float>(h >> 11) * (1.0 / 9007199254740992.0);

        h = splitmix64(h);

        const float u2 = static_cast<float>(h >> 11) * (1.0 / 9007199254740992.0);

        const float zNew = 1.0f - 2.0f * u1;
        const float phi = u2 * 2.0f * std::numbers::pi_v<float>;
        const float r = std::sqrt(std::max(0.0f, 1.0f - zNew * zNew));

        return {
            r * std::cos(phi),
            r * std::sin(phi),
            zNew
        };
    }

    // Knuths algorithm
    int poisson(float lambda, SeedType seed) {
        const float limit = glm::exp(-lambda);
        float prod = 1.0f;
        int k = 0;

        while (prod > limit) {
            ++k;
            prod *= toUnitFloat(hashValues(seed, k));
        }

        return k - 1;
    }

}
