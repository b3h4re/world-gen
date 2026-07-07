#pragma once

#include "config/app_config.hpp"
#include "files/exporter.hpp"
#include "terrain/generators/2d/generator_spec.hpp"
#include "terrain/generators/3d/generator_spec.hpp"

#include <functional>

namespace lve {

struct Callbacks {
    std::function<void()> regenerateTerrain;
    std::function<void()> reloadTerrain;
    std::function<void()> switchColor;
    std::function<void(wgen::GeneratorPipelineSpec)> pipelineChanged;
    std::function<void(wgen::Generator3dPipelineSpec)> planetPipelineChanged;
    std::function<wgen::GeneratorPipelineSpec()> currentPipeline;
    std::function<wgen::Generator3dPipelineSpec()> currentPlanetPipeline;
    std::function<wgen::AppConfig()> getConfig;
    std::function<void(wgen::WindowConfig)> configChanged;
    std::function<void(ExportConfig)> exportTerrain;
};


}
