#include "terrain_controls_widget.hpp"

#include "generator_settings_dialog.hpp"
#include "terrain_pipeline_list_model.hpp"
#include "ui_terrain_controls_widget.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QMenu>
#include <utility>

namespace lve {

TerrainControlsWidget::TerrainControlsWidget(Callbacks callbacks, QWidget* parent)
    : QWidget{parent}, ui_{std::make_unique<Ui::TerrainControlsWidget>()}, callbacks_{std::move(callbacks)} {
    ui_->setupUi(this);

    ui_->generatorSettingsWidget->setVisible(false);
    ui_->generatorSettingsButton->setCheckable(true);

    ui_->toolBarWidget->setMaximumWidth(256);

    wgen::GeneratorPipelineSpec initialPipeline;
    if (callbacks_.currentPipeline) {
        initialPipeline = callbacks_.currentPipeline();
    } else {
        initialPipeline = wgen::GeneratorPipelineSpec{
            wgen::GeneratorSpec{
                .kind = wgen::GeneratorKind::WorleyNoise,
                .config = wgen::WorleyNoiseGeneratorSpec{},
            },
        };
    }

    pipelineModel_ = std::make_unique<TerrainPipelineListModel>(std::move(initialPipeline));
    ui_->listView->setModel(pipelineModel_.get());

    auto notifyPipelineChanged = [this] {
        if (callbacks_.pipelineChanged) {
            callbacks_.pipelineChanged(pipelineModel_->pipeline());
        }
    };
    connect(pipelineModel_.get(), &QAbstractItemModel::modelReset, this, notifyPipelineChanged);
    connect(pipelineModel_.get(), &QAbstractItemModel::rowsInserted, this, notifyPipelineChanged);
    connect(pipelineModel_.get(), &QAbstractItemModel::rowsRemoved, this, notifyPipelineChanged);
    connect(
        pipelineModel_.get(),
        &QAbstractItemModel::dataChanged,
        this,
        [notifyPipelineChanged](const QModelIndex&, const QModelIndex&, const QList<int>&) {
            notifyPipelineChanged();
        });

    auto* addGeneratorMenu = new QMenu{ui_->addGeneratorButton};
    ui_->addGeneratorButton->setMenu(addGeneratorMenu);

    auto* addPerlinAction = addGeneratorMenu->addAction(QStringLiteral("Perlin"));
    connect(addPerlinAction, &QAction::triggered, this, [this] {
        pipelineModel_->appendGenerator(wgen::GeneratorSpec{
            .kind = wgen::GeneratorKind::PerlinNoise,
            .config = wgen::PerlinNoiseGeneratorSpec{
                .dotsPerCell = 100,
            },
            .scale = 1.0F,
        });
    });
    auto* addWorleyAction = addGeneratorMenu->addAction(QStringLiteral("Worley"));
    connect(addWorleyAction, &QAction::triggered, this, [this] {
        pipelineModel_->appendGenerator(wgen::GeneratorSpec{
            .kind = wgen::GeneratorKind::WorleyNoise,
            .config = wgen::WorleyNoiseGeneratorSpec{
                .dotsPerCell = 100,
                .p = 2.0F
            },
            .scale = 1.0F,
        });
    });
    auto* addValueNoiseAction = addGeneratorMenu->addAction(QStringLiteral("Value noise"));
    connect(addValueNoiseAction, &QAction::triggered, this, [this] {
        pipelineModel_->appendGenerator(wgen::GeneratorSpec{
            .kind = wgen::GeneratorKind::ValueNoise,
            .config = wgen::ValueNoiseGeneratorSpec{},
            .scale = 1.0F,
        });
    });

    connect(ui_->listView, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        const wgen::GeneratorSpec* spec = pipelineModel_->generatorAt(index.row());
        if (spec == nullptr) {
            return;
        }

        GeneratorSettingsDialog dialog{*spec, this};
        if (dialog.exec() == QDialog::Accepted) {
            pipelineModel_->updateGenerator(index.row(), dialog.generatorSpec());
        }
    });

    connect(ui_->regenerateButton, &QPushButton::clicked, this, [this] {
        callbacks_.regenerateTerrain();
    });
    connect(ui_->reloadSeedButton, &QPushButton::clicked, this, [this] {
        callbacks_.reloadTerrain();
    });
    connect(ui_->switchColorButton, &QPushButton::clicked, this, [this] {
        callbacks_.switchColor();
    });
    connect(ui_->generatorSettingsButton, &QPushButton::clicked, this, [this](bool checked) {
        ui_->generatorSettingsWidget->setVisible(checked);
    });
}

TerrainControlsWidget::~TerrainControlsWidget() = default;

QWidget& TerrainControlsWidget::vulkanWidget() {
    return *ui_->vulkanWidget;
}

} // namespace lve
