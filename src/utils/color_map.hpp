#pragma once


#include <cstdint>
#include <vector>
#include <functional>


#include <glm/glm.hpp>


namespace lve {

glm::vec3 terrainColor(float height);

glm::vec3 terrainBlackAndWhite(float height);

enum class ColorFunctions {
    BlackAndWhite,
    TerrainColorStandard
};

struct ColorMapParams {
    float minHeight;
    float maxHeight;
    std::uint32_t resolution;
    ColorFunctions mapping;
};


class ColorMapper {
public:
    static std::vector<std::uint8_t> generateTerrainColorMapRGBA8(ColorMapParams params);

    static std::function<glm::vec3(float)> getColorFunction(ColorFunctions f);

};

}
