#include "terrain/generators/generator.hpp"
#include "terrain/generators/noise/perlin.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace {

    bool mapsEqual(const wgen::HeightMap<float> &left, const wgen::HeightMap<float> &right) {
        if (left.width() != right.width() || left.height() != right.height()) {
            return false;
        }

        for (std::size_t y = 0; y < left.height(); ++y) {
            for (std::size_t x = 0; x < left.width(); ++x) {
                if (left.at(x, y) != right.at(x, y)) {
                    return false;
                }
            }
        }

        return true;
    }

    bool mapsDiffer(const wgen::HeightMap<float> &left, const wgen::HeightMap<float> &right) {
        return !mapsEqual(left, right);
    }

    void require(bool condition, const char *message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

}

int main() {
    static_assert(std::has_virtual_destructor_v<wgen::Generator>);

    try {
        constexpr wgen::SeedType seed = 12345;
        wgen::PerlinNoise2d first{5, 4, 8, seed};
        wgen::PerlinNoise2d second{5, 4, 8, seed};

        const auto firstMap = first.generateHeightMap(32, 24);
        const auto secondMap = second.generateHeightMap(32, 24);
        require(first.getSeed() == seed, "Perlin generator did not retain its seed");
        require(mapsEqual(firstMap, secondMap), "equal seeds produced different maps");

        second.setSeed(seed + 1);
        const auto differentMap = second.generateHeightMap(32, 24);
        require(mapsDiffer(firstMap, differentMap), "different seeds produced equal maps");

        second.setSeed(seed);
        const auto reseededMap = second.generateHeightMap(32, 24);
        require(mapsEqual(firstMap, reseededMap), "restoring a seed did not restore the map");

        std::unique_ptr<wgen::Generator> polymorphic =
            std::make_unique<wgen::PerlinNoise2d>(5, 4, 8, seed + 2);
        polymorphic->setSeed(seed);
        require(
            mapsEqual(firstMap, polymorphic->generateHeightMap(32, 24)),
            "polymorphic reseeding produced a different map");

        wgen::PerlinNoise2d minimal{2, 2, 2, seed};
        const auto minimalMap = minimal.generateHeightMap(2, 2);
        for (std::size_t y = 0; y < minimalMap.height(); ++y) {
            for (std::size_t x = 0; x < minimalMap.width(); ++x) {
                require(std::isfinite(minimalMap.at(x, y)), "Perlin map contains a non-finite value");
            }
        }

        bool rejectedOversizedMap = false;
        try {
            static_cast<void>(minimal.generateHeightMap(3, 2));
        } catch (const std::invalid_argument &) {
            rejectedOversizedMap = true;
        }
        require(rejectedOversizedMap, "oversized height map request was not rejected");

        bool rejectedUndersampledCells = false;
        try {
            static_cast<void>(wgen::PerlinNoise2d{2, 2, 1, seed});
        } catch (const std::invalid_argument &) {
            rejectedUndersampledCells = true;
        }
        require(rejectedUndersampledCells, "dotsPerCell below two was not rejected");
    } catch (const std::exception &exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
