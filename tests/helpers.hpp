#pragma once

#include "terrain/terrain.hpp"
#include "scripts/interpreter.hpp"

#include <string_view>


namespace wgen::tests {

using TestFunction = void (*)();
void runTest(std::string_view name, TestFunction test);

void require(bool condition, std::string_view message);

void expectNear(float actual, float expected, float epsilon, std::string_view message);

void expectMapNear(
    const wgen::HeightMap<float>& actual,
    const wgen::HeightMap<float>& expected,
    std::string_view message
);

template<typename Exception, typename Function>
void requireThrows(Function function, std::string_view message) {
    bool threwExpected = false;
    try {
        function();
    } catch (const Exception &) {
        threwExpected = true;
    }

    require(threwExpected, message);
}

template<typename T>
wgen::HeightMap<T> makeMap(std::size_t width, std::size_t height, const std::vector<T> &values) {
    require(values.size() == width * height, "test setup has wrong value count");

    wgen::HeightMap<T> map{width, height};
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            map.at(x, y) = values[y * width + x];
        }
    }

    return map;
}

template<typename T>
void expectValue(const wgen::Value &value, const T &expected, std::string_view message) {
    const auto *actual = std::get_if<T>(&value);
    require(actual != nullptr, message);
    require(*actual == expected, message);
}

}
