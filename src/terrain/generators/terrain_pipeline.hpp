#pragma once

#include "generator.hpp"

#include <initializer_list>
#include <string>
#include <vector>


namespace wgen {

    class TerrainPipeline : public Generator {
    public:
        TerrainPipeline() = default;
        explicit TerrainPipeline(std::vector<std::string> generatorLines);
        TerrainPipeline(std::initializer_list<std::string> generatorLines);

        void addGeneratorLine(std::string generatorLine);
        const std::vector<std::string>& generatorLines() const;

        HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) const override;
        float noise(std::size_t x, std::size_t y) const override;

    private:
        std::vector<std::string> generatorLines_{};

        std::string buildScript(std::size_t width, std::size_t height) const;
    };

}
