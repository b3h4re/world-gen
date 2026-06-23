#include "terrain/terrain.hpp"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <vector>


namespace {

    void require(bool condition, const char *message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    wgen::HeightMap<int> makeMap(std::size_t width, std::size_t height, const std::vector<int>& values) {
        require(values.size() == width * height, "test setup has wrong value count");

        wgen::HeightMap<int> map{width, height};
        for (std::size_t y = 0; y < height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {
                map.at(x, y) = values[y * width + x];
            }
        }

        return map;
    }

    void expectMapEquals(const wgen::HeightMap<int>& map, const std::vector<int>& expected) {
        require(expected.size() == map.width() * map.height(), "test setup has wrong expected value count");

        for (std::size_t y = 0; y < map.height(); ++y) {
            for (std::size_t x = 0; x < map.width(); ++x) {
                if (map.at(x, y) != expected[y * map.width() + x]) {
                    throw std::runtime_error("heightmap value mismatch");
                }
            }
        }
    }

}

int main() {
    try {
        {
            auto left = makeMap(3, 2, {
                1, 2, 3,
                4, 5, 6
            });
            auto right = makeMap(3, 2, {
                10, 20, 30,
                40, 50, 60
            });

            left += right;

            expectMapEquals(left, {
                11, 22, 33,
                44, 55, 66
            });
        }

        {
            const auto left = makeMap(3, 2, {
                1, 2, 3,
                4, 5, 6
            });
            const auto right = makeMap(3, 2, {
                10, 20, 30,
                40, 50, 60
            });

            const auto result = left + right;

            expectMapEquals(result, {
                11, 22, 33,
                44, 55, 66
            });
            expectMapEquals(left, {
                1, 2, 3,
                4, 5, 6
            });
            expectMapEquals(right, {
                10, 20, 30,
                40, 50, 60
            });
        }

        {
            auto base = makeMap(4, 3, {
                1, 1, 1, 1,
                1, 1, 1, 1,
                1, 1, 1, 1
            });
            const auto patch = makeMap(2, 2, {
                2, 3,
                4, 5
            });

            base.add_at(patch, 1, 1);

            expectMapEquals(base, {
                1, 1, 1, 1,
                1, 3, 4, 1,
                1, 5, 6, 1
            });
        }

        {
            auto base = makeMap(4, 3, {
                0, 0, 0, 0,
                0, 0, 0, 0,
                0, 0, 0, 0
            });
            const auto patch = makeMap(3, 2, {
                1, 2, 3,
                4, 5, 6
            });

            base.add_at(patch, 2, 2);

            expectMapEquals(base, {
                0, 0, 0, 0,
                0, 0, 0, 0,
                0, 0, 1, 2
            });
        }

        {
            auto base = makeMap(3, 2, {
                7, 7, 7,
                7, 7, 7
            });
            const auto patch = makeMap(2, 2, {
                1, 2,
                3, 4
            });

            base.add_at(patch, base.width(), 0);

            expectMapEquals(base, {
                7, 7, 7,
                7, 7, 7
            });
        }
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
