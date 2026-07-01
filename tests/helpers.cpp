#include "helpers.hpp"

#include "terrain/terrain.hpp"

#include <stdexcept>

namespace wgen::tests {

void runTest(std::string_view name, TestFunction test) {
    try {
        test();
    } catch (const std::exception &exception) {
        throw std::runtime_error{std::string{name} + ": " + exception.what()};
    }
}


void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error{std::string{message}};
    }
}

void expectNear(float actual, float expected, float epsilon, std::string_view message) {
    if (std::abs(actual - expected) > epsilon) {
        throw std::runtime_error{std::string{message}};
    }
}

void expectMapNear(
    const wgen::HeightMap<float>& actual,
    const wgen::HeightMap<float>& expected,
    std::string_view message
) {
    require(actual.width() == expected.width(), "heightmap width mismatch");
    require(actual.height() == expected.height(), "heightmap height mismatch");

    for (std::size_t y = 0; y < actual.height(); ++y) {
        for (std::size_t x = 0; x < actual.width(); ++x) {
            expectNear(actual.at(x, y), expected.at(x, y), 0.00001F, message);
        }
    }
}

}
