#include "terrain_app_gui.hpp"
#include "gui_helpers.hpp"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QWidget>
#include <utility>

namespace lve {

TerrainAppGui::TerrainAppGui(QWidget& parent, Callbacks callbacks) : parent_{parent} {
    auto* layout = new QHBoxLayout(&parent_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    controls_ = std::make_unique<TerrainControlsWidget>(callbacks, &parent_);
    layout->addWidget(controls_.get());
}

QWidget& TerrainAppGui::vulkanWidget() {
    return controls_->vulkanWidget();
}

} // namespace lve
