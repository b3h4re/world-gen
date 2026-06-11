#include "perlin.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <random>
#include <cassert>

namespace wgen {

float PerlinNoise2d::lerp(float a, float b, float c) {
    return (1 - c) * a + c * b;
}

float defaultPerlinInterp(float t) {
    return 6 * glm::pow(t, 5) - 15 * glm::pow(t, 4) + 10 * glm::pow(t, 3);
}

PerlinNoise2d::PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell, float (*funcInterpolate)(float)) {
    std::random_device rd;
    PerlinNoise2d(gridWidth, gridHeight, dotsPerCell, rd(), funcInterpolate);
}

PerlinNoise2d::PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell, std::uint32_t seed, float (*funcInterpolate)(float))
: funcInterpolate{funcInterpolate}, gridWidth{gridWidth}, dotsPerCell{dotsPerCell},
gridHeight{gridHeight}, grid{gridWidth, gridHeight}, cells{gridWidth - 1, gridHeight - 1} {
    setSeed(seed);

    std::mt19937 random{getSeed()};
    std::uniform_int_distribution<std::size_t> dist{0, 1110001};
    gridOffsetX = dist(random);
    gridOffsetY = dist(random);


    for (std::size_t y = 0; y < gridHeight; ++y) {
        for (std::size_t x = 0; x < gridWidth; ++x) {
            double hashVal = std::hash<glm::vec2>()(glm::vec2{x + gridOffsetX, y + gridOffsetY});
            grid.at(x, y) = glm::vec2{glm::cos(hashVal), glm::sin(hashVal)};
        }
    }

    for (std::size_t j = 0; j < gridHeight - 1; ++j) {
        for (std::size_t i = 0; i < gridWidth - 1; ++i) {
            HeightMap<glm::vec3> cell{dotsPerCell, dotsPerCell};

            for (std::size_t v = 0; v < dotsPerCell; ++v) {
                for (std::size_t u = 0; u < dotsPerCell; ++u) {
                    glm::vec2 p = glm::vec2(u, v) / (float)dotsPerCell;

                    glm::vec2 g_00 = grid.at(i, j);
                    glm::vec2 g_01 = grid.at(i, j + 1);
                    glm::vec2 g_10 = grid.at(i + 1, j);
                    glm::vec2 g_11 = grid.at(i + 1, j + 1);

                    glm::vec2 d_00{p.x, p.y};
                    glm::vec2 d_01{p.x, p.y - 1};
                    glm::vec2 d_10{p.x - 1, p.y};
                    glm::vec2 d_11{p.x - 1, p.y - 1};

                    float s_00 = glm::dot(d_00, g_00);
                    float s_01 = glm::dot(d_01, g_01);
                    float s_10 = glm::dot(d_10, g_10);
                    float s_11 = glm::dot(d_11, g_11);

                    float U = funcInterpolate(u);
                    float V = funcInterpolate(v);

                    // lerp(a, b, c) = (1 - c)a + cb

                    // a = lerp(s_00, s_10, U)
                    float a = lerp(s_00, s_10, U);
                    // b = lerp(s_01, s_11, V)
                    float b = lerp(s_01, s_11, V);
                    cell.at(u, v) = glm::vec3(a, b, V);
                }
            }
            cells.at(i, j) = cell;
        }
    }
}

float PerlinNoise2d::noise(std::size_t x, std::size_t y) {
    assert(x <= gridWidth * dotsPerCell & y <= gridHeight * dotsPerCell);
    std::size_t grid_x = x / dotsPerCell;
    std::size_t grid_y = y / dotsPerCell;

    std::size_t cell_x = x % dotsPerCell;
    std::size_t cell_y = y % dotsPerCell;

    glm::vec3 a_b_V = cells.at(grid_x, grid_y).at(cell_x, cell_y);
    return lerp(a_b_V.x, a_b_V.y, a_b_V.z);
}


HeightMap<float> PerlinNoise2d::generateheightMap(std::size_t width, std::size_t height) {
    assert(width <= gridWidth * dotsPerCell & height <= gridHeight * dotsPerCell);


    HeightMap<float> map{width, height};

    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            map.at(x, y) = noise(x, y);
        }
    }


    return map;
}

}
