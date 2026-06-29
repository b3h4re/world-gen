#include "terrain_app_gui.hpp"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>
#include <utility>

namespace lve {

TerrainAppGui::TerrainAppGui(QWidget& parent, Callbacks callbacks) : parent_{parent} {
    auto* layout = new QHBoxLayout(&parent_);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* regenerateButton = new QPushButton{"Regenerate", &parent_};
    auto* reloadButton = new QPushButton{"Reload seed", &parent_};
    auto* switchColorButton = new QPushButton{"Switch Color", &parent_};

    layout->addWidget(regenerateButton);
    layout->addWidget(reloadButton);
    layout->addWidget(switchColorButton);
    layout->addStretch(1);

    QObject::connect(regenerateButton, &QPushButton::clicked, &parent_, std::move(callbacks.regenerateTerrain));
    QObject::connect(reloadButton, &QPushButton::clicked, &parent_, std::move(callbacks.reloadTerrain));
    QObject::connect(switchColorButton, &QPushButton::clicked, &parent_, std::move(callbacks.switchColor));
}

} // namespace lve
