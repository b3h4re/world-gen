#include "terrain_controls_widget.hpp"

#include "ui_terrain_controls_widget.h"

#include <utility>

namespace lve {

TerrainControlsWidget::TerrainControlsWidget(Callbacks callbacks, QWidget* parent)
    : QWidget{parent}, ui_{std::make_unique<Ui::TerrainControlsWidget>()}, callbacks_{std::move(callbacks)} {
    ui_->setupUi(this);

    connect(ui_->regenerateButton, &QPushButton::clicked, this, [this] {
        callbacks_.regenerateTerrain();
    });
    connect(ui_->reloadSeedButton, &QPushButton::clicked, this, [this] {
        callbacks_.reloadTerrain();
    });
    connect(ui_->switchColorButton, &QPushButton::clicked, this, [this] {
        callbacks_.switchColor();
    });
}

TerrainControlsWidget::~TerrainControlsWidget() = default;

} // namespace lve
