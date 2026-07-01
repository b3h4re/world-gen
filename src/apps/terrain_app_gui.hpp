#pragma once

#include "apps/terrain_controls_widget.hpp"
#include "terrain/generators/generator_spec.hpp"
#include "terrain/terrain_compute_method.hpp"

#include <functional>
#include <memory>

class QWidget;

namespace lve {

class TerrainAppGui {
public:
    struct Callbacks {
        std::function<void()> regenerateTerrain;
        std::function<void()> reloadTerrain;
        std::function<void()> switchColor;
        std::function<void(wgen::GeneratorPipelineSpec)> pipelineChanged;
        std::function<wgen::GeneratorPipelineSpec()> currentPipeline;
        std::function<void(wgen::TerrainComputeMethod)> computeMethodChanged;
        std::function<wgen::TerrainComputeMethod()> currentComputeMethod;
    };

    TerrainAppGui(QWidget& parent, Callbacks callbacks);

    TerrainAppGui(const TerrainAppGui&) = delete;
    TerrainAppGui& operator=(const TerrainAppGui&) = delete;

    QWidget& vulkanWidget();

private:
    QWidget& parent_;
    std::unique_ptr<TerrainControlsWidget> controls_;
};

} // namespace lve
