#include "terrain_pipeline.hpp"

#include "scripts/interpreter.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>


namespace wgen {

namespace {

    std::string trim(std::string text) {
        const auto first = std::find_if_not(
            text.begin(),
            text.end(),
            [](unsigned char c) {
                return std::isspace(c) != 0;
            }
        );

        const auto last = std::find_if_not(
            text.rbegin(),
            text.rend(),
            [](unsigned char c) {
                return std::isspace(c) != 0;
            }
        ).base();

        if (first >= last) {
            return {};
        }

        return {first, last};
    }

    std::filesystem::path writeTemporaryPipelineScript(const std::string& script) {
        const auto path = std::filesystem::temp_directory_path() / "world_gen_terrain_pipeline_script.txt";
        std::ofstream file{path};
        if (!file) {
            throw std::runtime_error("failed to create terrain pipeline interpreter script");
        }

        file << script;
        return path;
    }

}

    TerrainPipeline::TerrainPipeline(std::vector<std::string> generatorLines)
    : generatorLines_{std::move(generatorLines)} {}

    TerrainPipeline::TerrainPipeline(std::initializer_list<std::string> generatorLines)
    : generatorLines_{generatorLines} {}

    void TerrainPipeline::addGeneratorLine(std::string generatorLine) {
        generatorLines_.push_back(std::move(generatorLine));
    }

    const std::vector<std::string>& TerrainPipeline::generatorLines() const {
        return generatorLines_;
    }

    HeightMap<float> TerrainPipeline::generateHeightMap(std::size_t width, std::size_t height) const {
        Interpreter interpreter;
        interpreter.loadScript(writeTemporaryPipelineScript(buildScript(width, height)));
        interpreter.executeScript();

        return as<HeightMap<float>>(interpreter.getVariableValue("h"));
    }

    float TerrainPipeline::noise(std::size_t, std::size_t) const {
        throw std::runtime_error("TerrainPipeline does not support point noise generation");
    }

    std::string TerrainPipeline::buildScript(std::size_t width, std::size_t height) const {
        std::ostringstream script;
        script << "decl heightmap h width=" << width << " height=" << height << '\n';

        std::size_t generatorIndex = 1;
        for (const std::string& rawLine : generatorLines_) {
            const std::string line = trim(rawLine);
            if (line.empty()) {
                continue;
            }

            std::istringstream lineStream{line};
            std::string generatorType;
            lineStream >> generatorType;

            std::string generatorArguments;
            std::getline(lineStream, generatorArguments);
            generatorArguments = trim(generatorArguments);

            const std::string generatorName = "gen" + std::to_string(generatorIndex);
            const std::string generatorResultName = generatorName + "_res";

            script << "decl " << generatorType << ' ' << generatorName;
            if (!generatorArguments.empty()) {
                script << ' ' << generatorArguments;
            }
            script << '\n';

            script << "decl heightmap " << generatorResultName
                   << " width=" << width
                   << " height=" << height
                   << '\n';
            script << "add " << generatorResultName << " to " << generatorName << " = " << generatorResultName << '\n';
            script << "add " << generatorResultName << " to h = h\n";

            ++generatorIndex;
        }

        return script.str();
    }

}
