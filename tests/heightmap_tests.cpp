#include "terrain/terrain.hpp"

#include <glm/glm.hpp>

#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error{std::string{message}};
    }
}

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
void expectMapEquals(const wgen::HeightMap<T> &map, const std::vector<T> &expected, std::string_view message) {
    require(expected.size() == map.width() * map.height(), "test setup has wrong expected value count");

    for (std::size_t y = 0; y < map.height(); ++y) {
        for (std::size_t x = 0; x < map.width(); ++x) {
            if (map.at(x, y) != expected[y * map.width() + x]) {
                throw std::runtime_error{std::string{message}};
            }
        }
    }
}

void expectNear(float actual, float expected, float epsilon, std::string_view message) {
    if (std::abs(actual - expected) > epsilon) {
        throw std::runtime_error{std::string{message}};
    }
}

void testDefaultConstruction() {
    const wgen::HeightMap<int> map{};

    require(map.width() == 0, "default heightmap width should be zero");
    require(map.height() == 0, "default heightmap height should be zero");
    require(map.at(0, 0) == 0, "default map currently exposes one default-initialized sample");
    requireThrows<std::invalid_argument>([&] { static_cast<void>(std::min(map)); }, "min(empty map) should throw");
    requireThrows<std::invalid_argument>([&] { static_cast<void>(std::max(map)); }, "max(empty map) should throw");
}

void testConstructionAndInitialization() {
    wgen::HeightMap<int> map{3, 2, 7};
    require(map.width() == 3, "constructed heightmap has wrong width");
    require(map.height() == 2, "constructed heightmap has wrong height");
    expectMapEquals(map, {7, 7, 7, 7, 7, 7}, "default-filled heightmap has wrong values");

    const wgen::HeightMap<int> fromRows{{1, 2, 3}, {4, 5, 6}};
    expectMapEquals(fromRows, {1, 2, 3, 4, 5, 6}, "initializer-list heightmap has wrong values");

    requireThrows<std::invalid_argument>([] { wgen::HeightMap<int>{1, 2}; }, "width below two should throw");
    requireThrows<std::invalid_argument>([] { wgen::HeightMap<int>{2, 1}; }, "height below two should throw");
    requireThrows<std::invalid_argument>([] { wgen::HeightMap<int>{{1, 2}, {3}}; }, "ragged initializer should throw");
    requireThrows<std::invalid_argument>([] { wgen::HeightMap<int>{{1, 2}}; }, "one-row initializer should throw");
}

void testElementAccess() {
    auto map = makeMap<int>(3, 2, {1, 2, 3, 4, 5, 6});

    require(map.at(2, 1) == 6, "coordinate access returned wrong value");
    require(map.at(glm::ivec2{1, 1}) == 5, "glm coordinate access returned wrong value");

    map.at(0, 1) = 40;
    map.at(glm::ivec2{2, 0}) = 30;
    expectMapEquals(map, {1, 2, 30, 40, 5, 6}, "mutable element access wrote wrong values");

    const auto &constMap = map;
    require(constMap.at(glm::ivec2{2, 0}) == 30, "const glm coordinate access returned wrong value");
    require(map.at(3, 0) == 40, "at(x, y) currently bounds-checks only the flattened index");
    requireThrows<std::out_of_range>([&] { static_cast<void>(map.at(0, 2)); }, "y out-of-range access should throw");
}

void testCopyAndMove() {
    const auto original = makeMap<int>(3, 2, {1, 2, 3, 4, 5, 6});

    auto copied = original;
    copied.at(0, 0) = 99;
    expectMapEquals(original, {1, 2, 3, 4, 5, 6}, "copy should not mutate original");
    expectMapEquals(copied, {99, 2, 3, 4, 5, 6}, "copy has wrong values after mutation");

    auto moved = std::move(copied);
    expectMapEquals(moved, {99, 2, 3, 4, 5, 6}, "moved map has wrong values");

    wgen::HeightMap<int> assigned{2, 2, 0};
    assigned = original;
    expectMapEquals(assigned, {1, 2, 3, 4, 5, 6}, "copy assignment has wrong values");
}

void testHeightMapAddition() {
    auto left = makeMap<int>(3, 2, {1, 2, 3, 4, 5, 6});
    const auto right = makeMap<int>(3, 2, {10, 20, 30, 40, 50, 60});

    left += right;
    expectMapEquals(left, {11, 22, 33, 44, 55, 66}, "operator+= heightmap result is wrong");

    const auto sum = makeMap<int>(3, 2, {1, 2, 3, 4, 5, 6}) + right;
    expectMapEquals(sum, {11, 22, 33, 44, 55, 66}, "operator+ heightmap result is wrong");
    expectMapEquals(right, {10, 20, 30, 40, 50, 60}, "operator+ should not mutate rhs");

    auto ints = makeMap<int>(2, 2, {1, 1, 1, 1});
    const auto floats = makeMap<float>(2, 2, {0.4F, 1.6F, 2.2F, 3.9F});
    ints += floats;
    expectMapEquals(ints, {1, 2, 3, 4}, "mixed-type heightmap addition should cast values");
}

void testAddAtCropping() {
    auto base = makeMap<int>(4, 3, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
    const auto patch = makeMap<int>(2, 2, {2, 3, 4, 5});

    base.add_at(patch, 1, 1);
    expectMapEquals(base, {1, 1, 1, 1, 1, 3, 4, 1, 1, 5, 6, 1}, "add_at centered patch result is wrong");

    auto cropped = makeMap<int>(4, 3, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    const auto widePatch = makeMap<int>(3, 2, {1, 2, 3, 4, 5, 6});
    cropped.add_at(widePatch, 2, 2);
    expectMapEquals(cropped, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2}, "add_at cropped patch result is wrong");

    auto unchanged = makeMap<int>(3, 2, {7, 7, 7, 7, 7, 7});
    unchanged.add_at(patch, unchanged.width(), 0);
    expectMapEquals(unchanged, {7, 7, 7, 7, 7, 7}, "add_at outside width should not mutate map");
    unchanged.add_at(patch, 0, unchanged.height());
    expectMapEquals(unchanged, {7, 7, 7, 7, 7, 7}, "add_at outside height should not mutate map");
}

void testScalarArithmetic() {
    auto map = makeMap<int>(3, 2, {1, 2, 3, 4, 5, 6});

    map += 10;
    expectMapEquals(map, {11, 12, 13, 14, 15, 16}, "operator+= scalar result is wrong");

    const auto plusRight = map + 1;
    expectMapEquals(plusRight, {12, 13, 14, 15, 16, 17}, "heightmap + scalar result is wrong");

    const auto plusLeft = 1 + map;
    expectMapEquals(plusLeft, {12, 13, 14, 15, 16, 17}, "scalar + heightmap result is wrong");

    map -= 1;
    expectMapEquals(map, {10, 11, 12, 13, 14, 15}, "operator-= scalar result is wrong");

    const auto minusRight = map - 10;
    expectMapEquals(minusRight, {0, 1, 2, 3, 4, 5}, "heightmap - scalar result is wrong");

    const auto minusLeft = 20 - map;
    expectMapEquals(minusLeft, {10, 9, 8, 7, 6, 5}, "scalar - heightmap result is wrong");

    map *= 2;
    expectMapEquals(map, {20, 22, 24, 26, 28, 30}, "operator*= scalar result is wrong");

    const auto multiplyRight = map * 2;
    expectMapEquals(multiplyRight, {40, 44, 48, 52, 56, 60}, "heightmap * scalar result is wrong");

    const auto multiplyLeft = 2 * map;
    expectMapEquals(multiplyLeft, {40, 44, 48, 52, 56, 60}, "scalar * heightmap result is wrong");
}

void testClearMinMax() {
    auto map = makeMap<int>(3, 2, {4, -2, 9, 0, 7, 1});

    require(std::min(map) == -2, "std::min heightmap result is wrong");
    require(std::max(map) == 9, "std::max heightmap result is wrong");

    map.clear(5);
    expectMapEquals(map, {5, 5, 5, 5, 5, 5}, "clear result is wrong");
    require(std::min(map) == 5, "std::min after clear is wrong");
    require(std::max(map) == 5, "std::max after clear is wrong");
}

void testProjectionAndNormalization() {
    auto map = makeMap<float>(2, 2, {0.0F, 5.0F, 10.0F, 15.0F});

    map.project(0.0F, 1.0F);
    expectNear(map.at(0, 0), 0.0F, 0.00001F, "project min result is wrong");
    expectNear(map.at(1, 0), 1.0F / 3.0F, 0.00001F, "project middle result is wrong");
    expectNear(map.at(0, 1), 2.0F / 3.0F, 0.00001F, "project middle result is wrong");
    expectNear(map.at(1, 1), 1.0F, 0.00001F, "project max result is wrong");

    const auto normalized = makeMap<float>(2, 2, {0.0F, 5.0F, 10.0F, 15.0F}).normal();
    expectNear(normalized.at(0, 0), -1.0F, 0.00001F, "normal min result is wrong");
    expectNear(normalized.at(1, 1), 1.0F, 0.00001F, "normal max result is wrong");

    auto original = makeMap<float>(2, 2, {2.0F, 4.0F, 6.0F, 8.0F});
    const auto projected = original.projected(10.0F, 20.0F);
    expectNear(projected.at(0, 0), 10.0F, 0.00001F, "projected min result is wrong");
    expectNear(projected.at(1, 1), 20.0F, 0.00001F, "projected max result is wrong");
    expectNear(original.at(0, 0), 2.0F, 0.00001F, "projected should not mutate original");

    auto constant = makeMap<float>(2, 2, {3.0F, 3.0F, 3.0F, 3.0F});
    constant.project(-2.0F, 2.0F);
    expectNear(constant.at(0, 0), 0.0F, 0.00001F, "constant project should use target midpoint");
    expectNear(constant.at(1, 1), 0.0F, 0.00001F, "constant project should use target midpoint");
}

using TestFunction = void (*)();

void runTest(std::string_view name, TestFunction test) {
    try {
        test();
    } catch (const std::exception &exception) {
        throw std::runtime_error{std::string{name} + ": " + exception.what()};
    }
}

} // namespace

int main() {
    try {
        runTest("default construction", testDefaultConstruction);
        runTest("construction and initialization", testConstructionAndInitialization);
        runTest("element access", testElementAccess);
        runTest("copy and move", testCopyAndMove);
        runTest("heightmap addition", testHeightMapAddition);
        runTest("add_at cropping", testAddAtCropping);
        runTest("scalar arithmetic", testScalarArithmetic);
        runTest("clear min max", testClearMinMax);
        runTest("projection and normalization", testProjectionAndNormalization);
    } catch (const std::exception &exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
