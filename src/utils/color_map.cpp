#include "color_map.hpp"
#include "terrain/generators/generator.hpp"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <stdexcept>
#include <stdexcept>

namespace lve {


/*
So first map [-1; 1] to half circle in complex plain
        -1 |---> -i (blue)
         0 |--->  1 (green)
         1 |--->  i (red)
Which is w(x) = (i - x) / (i + x)
So we get a vector in 2d space with
Re(w) = (1 - x^2) / (1 + x^2)
Im(w) = 2 x / (1 + x^2)

Then convert the resulting vector to HSV and then to RGB
*/
glm::vec3 terrainColor(float height) {
    if (std::abs(height) > 1.0f) {
        throw std::runtime_error("height must be in [-1; 1] to get color mapping.");
    }

    float re = (1 - height * height) / (1 + height * height);
    float im = 2 * height / (1 + height * height);

    float theta = std::atan2(im, re);

    // convert to HSV first, S = 1, V = 1
    // H' = 6H
    float H_ = 6 * (0.333f - 2 * theta * 0.333f / glm::pi<float>());

    // temp value for conversion
    float X = 1 - std::abs(glm::mod<float>(H_, 2) - 1);

    if (H_ < 1) {
        return glm::vec3{1.0f, X, 0.0f};
    }

    if (H_ < 2) {
        return glm::vec3{X, 1.0f, 0.0f};
    }

    if (H_ < 3) {
        return glm::vec3{0.0f, 1.0f, X};
    }

    if (H_ < 4) {
        return glm::vec3{0.0f, X, 1.0f};
    }

    if (H_ < 5) {
        return glm::vec3{X, 0.0f, 1.0f};
    }

    return glm::vec3{1.0f, 0.0f, X};

}


glm::vec3 terrainBlackAndWhite(float height) {
    if (std::abs(height) > 1.0f) {
        throw std::runtime_error("height must be in [-1; 1] to get color mapping.");
    }

    return glm::vec3{(height + 1) / 2};
}

std::vector<std::uint8_t> ColorMapper::generateTerrainColorMapRGBA8(ColorMapParams params) {
    std::vector<std::uint8_t> pixels(params.resolution * 4);
    auto colorFunc = getColorFunction(params.mapping);
    for (std::uint32_t i = 0; i < params.resolution; ++i) {
        float t = static_cast<float>(i) /
                  static_cast<float>(params.resolution - 1);

        float height = wgen::lerp(params.minHeight, params.maxHeight, t);

        glm::vec3 color = colorFunc(height);

        color = glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f));

        pixels[i * 4 + 0] = static_cast<std::uint8_t>(color.r * 255.0f);
        pixels[i * 4 + 1] = static_cast<std::uint8_t>(color.g * 255.0f);
        pixels[i * 4 + 2] = static_cast<std::uint8_t>(color.b * 255.0f);
        pixels[i * 4 + 3] = 255;
    }
    return pixels;
}

std::function<glm::vec3(float)> ColorMapper::getColorFunction(ColorFunctions f) {
    switch (f) {
        case ColorFunctions::BlackAndWhite:
            return terrainBlackAndWhite;
        case ColorFunctions::TerrainColorStandard:
            return terrainColor;
    }

    throw std::runtime_error("Unsupported color function encountered.");
}

}
