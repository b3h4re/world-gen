#pragma once

#include "terrain/generators/generator_spec.hpp"
#include "terrain/terrain_compute_method.hpp"

#include <QWidget>

#include <functional>
#include <memory>

namespace Ui {
class TerrainControlsWidget;
}

namespace lve {

class TerrainPipelineListModel;

class TerrainControlsWidget : public QWidget {
    Q_OBJECT

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

    explicit TerrainControlsWidget(Callbacks callbacks, QWidget* parent = nullptr);
    ~TerrainControlsWidget() override;

    TerrainControlsWidget(const TerrainControlsWidget&) = delete;
    TerrainControlsWidget& operator=(const TerrainControlsWidget&) = delete;

    QWidget& vulkanWidget();

private:
    std::unique_ptr<Ui::TerrainControlsWidget> ui_;
    std::unique_ptr<TerrainPipelineListModel> pipelineModel_;
    Callbacks callbacks_;
};

} // namespace lve
