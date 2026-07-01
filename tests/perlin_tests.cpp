#include "terrain/generators/generator.hpp"
#include "terrain/generators/noise/perlin.hpp"

#include "helpers.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>


int main() {
    static_assert(std::has_virtual_destructor_v<wgen::Generator>);

    try {
        constexpr wgen::SeedType seed = 12345;
        wgen::PerlinNoise2d first{8, seed};
        wgen::PerlinNoise2d second{8, seed};

        const auto firstMap = first.generateHeightMap(32, 24);
        const auto secondMap = second.generateHeightMap(32, 24);
        wgen::tests::require(first.getSeed() == seed, "Perlin generator did not retain its seed");
        wgen::tests::require(firstMap == secondMap, "equal seeds produced different maps");

        second.setSeed(seed + 1);
        const auto differentMap = second.generateHeightMap(32, 24);
        wgen::tests::require(firstMap != differentMap, "different seeds produced equal maps");

        second.setSeed(seed);
        const auto reseededMap = second.generateHeightMap(32, 24);
        wgen::tests::require(firstMap == reseededMap, "restoring a seed did not restore the map");

        std::unique_ptr<wgen::Generator> polymorphic =
            std::make_unique<wgen::PerlinNoise2d>(8, seed + 2);
        polymorphic->setSeed(seed);
        wgen::tests::require(
            firstMap == polymorphic->generateHeightMap(32, 24),
            "polymorphic reseeding produced a different map");

        wgen::PerlinNoise2d minimal{2, seed};
        const auto arbitraryMap = minimal.generateHeightMap(33, 25);
        for (std::size_t y = 0; y < arbitraryMap.height(); ++y) {
            for (std::size_t x = 0; x < arbitraryMap.width(); ++x) {
                wgen::tests::require(std::isfinite(arbitraryMap.at(x, y)), "Perlin map contains a non-finite value");
            }
        }

        bool rejectedUndersampledCells = false;
        try {
            static_cast<void>(wgen::PerlinNoise2d{1, seed});
        } catch (const std::invalid_argument &) {
            rejectedUndersampledCells = true;
        }
        wgen::tests::require(rejectedUndersampledCells, "dotsPerCell below two was not rejected");
    } catch (const std::exception &exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
