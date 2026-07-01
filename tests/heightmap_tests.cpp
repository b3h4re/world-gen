#include "terrain/terrain.hpp"

#include "helpers.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {



template<typename T>
void expectMapEquals(const wgen::HeightMap<T> &map, const std::vector<T> &expected, std::string_view message) {
    wgen::tests::require(expected.size() == map.width() * map.height(), "test setup has wrong expected value count");

    for (std::size_t y = 0; y < map.height(); ++y) {
        for (std::size_t x = 0; x < map.width(); ++x) {
            if (map.at(x, y) != expected[y * map.width() + x]) {
                throw std::runtime_error{std::string{message}};
            }
        }
    }
}

void testDefaultConstruction() {
    const wgen::HeightMap<int> map{};

    wgen::tests::require(map.width() == 0, "default heightmap width should be zero");
    wgen::tests::require(map.height() == 0, "default heightmap height should be zero");
    wgen::tests::require(map.at(0, 0) == 0, "default map currently exposes one default-initialized sample");
    wgen::tests::requireThrows<std::invalid_argument>([&] { static_cast<void>(std::min(map)); }, "min(empty map) should throw");
    wgen::tests::requireThrows<std::invalid_argument>([&] { static_cast<void>(std::max(map)); }, "max(empty map) should throw");
}

void testConstructionAndInitialization() {
    wgen::HeightMap<int> map{3, 2, 7};
    wgen::tests::require(map.width() == 3, "constructed heightmap has wrong width");
    wgen::tests::require(map.height() == 2, "constructed heightmap has wrong height");
    expectMapEquals(map, {7, 7, 7, 7, 7, 7}, "default-filled heightmap has wrong values");

    const wgen::HeightMap<int> fromRows{{1, 2, 3}, {4, 5, 6}};
    expectMapEquals(fromRows, {1, 2, 3, 4, 5, 6}, "initializer-list heightmap has wrong values");

    wgen::tests::requireThrows<std::invalid_argument>([] { wgen::HeightMap<int>{1, 2}; }, "width below two should throw");
    wgen::tests::requireThrows<std::invalid_argument>([] { wgen::HeightMap<int>{2, 1}; }, "height below two should throw");
    wgen::tests::requireThrows<std::invalid_argument>([] { wgen::HeightMap<int>{{1, 2}, {3}}; }, "ragged initializer should throw");
    wgen::tests::requireThrows<std::invalid_argument>([] { wgen::HeightMap<int>{{1, 2}}; }, "one-row initializer should throw");
}

void testElementAccess() {
    auto map = wgen::tests::makeMap<int>(3, 2, {1, 2, 3, 4, 5, 6});

    wgen::tests::require(map.at(2, 1) == 6, "coordinate access returned wrong value");
    wgen::tests::require(map.at(glm::ivec2{1, 1}) == 5, "glm coordinate access returned wrong value");

    map.at(0, 1) = 40;
    map.at(glm::ivec2{2, 0}) = 30;
    expectMapEquals(map, {1, 2, 30, 40, 5, 6}, "mutable element access wrote wrong values");

    const auto &constMap = map;
    wgen::tests::require(constMap.at(glm::ivec2{2, 0}) == 30, "const glm coordinate access returned wrong value");
    wgen::tests::require(map.at(3, 0) == 40, "at(x, y) currently bounds-checks only the flattened index");
    wgen::tests::requireThrows<std::out_of_range>([&] { static_cast<void>(map.at(0, 2)); }, "y out-of-range access should throw");
}

void testCopyAndMove() {
    const auto original = wgen::tests::makeMap<int>(3, 2, {1, 2, 3, 4, 5, 6});

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
    auto left = wgen::tests::makeMap<int>(3, 2, {1, 2, 3, 4, 5, 6});
    const auto right = wgen::tests::makeMap<int>(3, 2, {10, 20, 30, 40, 50, 60});

    left += right;
    expectMapEquals(left, {11, 22, 33, 44, 55, 66}, "operator+= heightmap result is wrong");

    const auto sum = wgen::tests::makeMap<int>(3, 2, {1, 2, 3, 4, 5, 6}) + right;
    expectMapEquals(sum, {11, 22, 33, 44, 55, 66}, "operator+ heightmap result is wrong");
    expectMapEquals(right, {10, 20, 30, 40, 50, 60}, "operator+ should not mutate rhs");

    auto ints = wgen::tests::makeMap<int>(2, 2, {1, 1, 1, 1});
    const auto floats = wgen::tests::makeMap<float>(2, 2, {0.4F, 1.6F, 2.2F, 3.9F});
    ints += floats;
    expectMapEquals(ints, {1, 2, 3, 4}, "mixed-type heightmap addition should cast values");
}

void testAddAtCropping() {
    auto base = wgen::tests::makeMap<int>(4, 3, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
    const auto patch = wgen::tests::makeMap<int>(2, 2, {2, 3, 4, 5});

    base.add_at(patch, 1, 1);
    expectMapEquals(base, {1, 1, 1, 1, 1, 3, 4, 1, 1, 5, 6, 1}, "add_at centered patch result is wrong");

    auto cropped = wgen::tests::makeMap<int>(4, 3, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    const auto widePatch = wgen::tests::makeMap<int>(3, 2, {1, 2, 3, 4, 5, 6});
    cropped.add_at(widePatch, 2, 2);
    expectMapEquals(cropped, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2}, "add_at cropped patch result is wrong");

    auto unchanged = wgen::tests::makeMap<int>(3, 2, {7, 7, 7, 7, 7, 7});
    unchanged.add_at(patch, unchanged.width(), 0);
    expectMapEquals(unchanged, {7, 7, 7, 7, 7, 7}, "add_at outside width should not mutate map");
    unchanged.add_at(patch, 0, unchanged.height());
    expectMapEquals(unchanged, {7, 7, 7, 7, 7, 7}, "add_at outside height should not mutate map");
}

void testScalarArithmetic() {
    auto map = wgen::tests::makeMap<int>(3, 2, {1, 2, 3, 4, 5, 6});

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
    auto map = wgen::tests::makeMap<int>(3, 2, {4, -2, 9, 0, 7, 1});

    wgen::tests::require(std::min(map) == -2, "std::min heightmap result is wrong");
    wgen::tests::require(std::max(map) == 9, "std::max heightmap result is wrong");

    map.clear(5);
    expectMapEquals(map, {5, 5, 5, 5, 5, 5}, "clear result is wrong");
    wgen::tests::require(std::min(map) == 5, "std::min after clear is wrong");
    wgen::tests::require(std::max(map) == 5, "std::max after clear is wrong");
}

void testProjectionAndNormalization() {
    auto map = wgen::tests::makeMap<float>(2, 2, {0.0F, 5.0F, 10.0F, 15.0F});

    map.project(0.0F, 1.0F);
    wgen::tests::expectNear(map.at(0, 0), 0.0F, 0.00001F, "project min result is wrong");
    wgen::tests::expectNear(map.at(1, 0), 1.0F / 3.0F, 0.00001F, "project middle result is wrong");
    wgen::tests::expectNear(map.at(0, 1), 2.0F / 3.0F, 0.00001F, "project middle result is wrong");
    wgen::tests::expectNear(map.at(1, 1), 1.0F, 0.00001F, "project max result is wrong");

    const auto normalized = wgen::tests::makeMap<float>(2, 2, {0.0F, 5.0F, 10.0F, 15.0F}).normal();
    wgen::tests::expectNear(normalized.at(0, 0), -1.0F, 0.00001F, "normal min result is wrong");
    wgen::tests::expectNear(normalized.at(1, 1), 1.0F, 0.00001F, "normal max result is wrong");

    auto original = wgen::tests::makeMap<float>(2, 2, {2.0F, 4.0F, 6.0F, 8.0F});
    const auto projected = original.projected(10.0F, 20.0F);
    wgen::tests::expectNear(projected.at(0, 0), 10.0F, 0.00001F, "projected min result is wrong");
    wgen::tests::expectNear(projected.at(1, 1), 20.0F, 0.00001F, "projected max result is wrong");
    wgen::tests::expectNear(original.at(0, 0), 2.0F, 0.00001F, "projected should not mutate original");

    auto constant = wgen::tests::makeMap<float>(2, 2, {3.0F, 3.0F, 3.0F, 3.0F});
    constant.project(-2.0F, 2.0F);
    wgen::tests::expectNear(constant.at(0, 0), 0.0F, 0.00001F, "constant project should use target midpoint");
    wgen::tests::expectNear(constant.at(1, 1), 0.0F, 0.00001F, "constant project should use target midpoint");
}


} // namespace

int main() {
    try {
        wgen::tests::runTest("default construction", testDefaultConstruction);
        wgen::tests::runTest("construction and initialization", testConstructionAndInitialization);
        wgen::tests::runTest("element access", testElementAccess);
        wgen::tests::runTest("copy and move", testCopyAndMove);
        wgen::tests::runTest("heightmap addition", testHeightMapAddition);
        wgen::tests::runTest("add_at cropping", testAddAtCropping);
        wgen::tests::runTest("scalar arithmetic", testScalarArithmetic);
        wgen::tests::runTest("clear min max", testClearMinMax);
        wgen::tests::runTest("projection and normalization", testProjectionAndNormalization);
    } catch (const std::exception &exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
