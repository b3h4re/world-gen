#include "terrain_app_gui.hpp"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QWidget>
#include <utility>

namespace lve {

TerrainAppGui::TerrainAppGui(QWidget& parent, Callbacks callbacks) : parent_{parent} {
    auto* layout = new QHBoxLayout(&parent_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    controls_ = std::make_unique<TerrainControlsWidget>(
        TerrainControlsWidget::Callbacks{
            .regenerateTerrain = std::move(callbacks.regenerateTerrain),
            .reloadTerrain = std::move(callbacks.reloadTerrain),
            .switchColor = std::move(callbacks.switchColor),
            .pipelineChanged = std::move(callbacks.pipelineChanged),
            .currentPipeline = std::move(callbacks.currentPipeline),
            .computeMethodChanged = std::move(callbacks.computeMethodChanged),
            .currentComputeMethod = std::move(callbacks.currentComputeMethod),
        },
        &parent_);
    layout->addWidget(controls_.get());
}

QWidget& TerrainAppGui::vulkanWidget() {
    return controls_->vulkanWidget();
}

} // namespace lve
