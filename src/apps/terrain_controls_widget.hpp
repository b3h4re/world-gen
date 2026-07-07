#pragma once

#include "terrain/generators/2d/generator_spec.hpp"
#include "config/app_config.hpp"
#include "gui_helpers.hpp"

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
    explicit TerrainControlsWidget(Callbacks callbacks, QWidget* parent = nullptr);
    ~TerrainControlsWidget() override;

    TerrainControlsWidget(const TerrainControlsWidget&) = delete;
    TerrainControlsWidget& operator=(const TerrainControlsWidget&) = delete;

    QWidget& vulkanWidget();

private:
    std::unique_ptr<Ui::TerrainControlsWidget> ui_;
    std::unique_ptr<TerrainPipelineListModel> pipelineModel_;
    Callbacks callbacks_;

    void clearPipeline();

};

} // namespace lve
