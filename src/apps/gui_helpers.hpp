#pragma once

#include "config/app_config.hpp"
#include "terrain/generators/generator_spec.hpp"

#include <functional>

namespace lve {

struct Callbacks {
    std::function<void()> regenerateTerrain;
    std::function<void()> reloadTerrain;
    std::function<void()> switchColor;
    std::function<void(wgen::GeneratorPipelineSpec)> pipelineChanged;
    std::function<wgen::GeneratorPipelineSpec()> currentPipeline;
    std::function<wgen::AppConfig()> getConfig;
    std::function<void(wgen::AppConfig)> configChanged;
};


}
