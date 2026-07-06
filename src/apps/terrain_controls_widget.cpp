#include "terrain_controls_widget.hpp"

#include "generator_settings_dialog.hpp"
#include "app_settings.hpp"
#include "save_terrain.hpp"
#include "files/exporter.hpp"
#include "terrain_pipeline_list_model.hpp"
#include "ui_terrain_controls_widget.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QListView>
#include <QMenu>
#include <QPoint>
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
    auto* addGaborAction = addGeneratorMenu->addAction(QStringLiteral("Gabor"));
    connect(addGaborAction, &QAction::triggered, this, [this] {
        wgen::GaborNoiseGeneratorSpec specDefault{};
        pipelineModel_->appendGenerator(wgen::GeneratorSpec{
            .kind = wgen::GeneratorKind::GaborNoise,
            .config = wgen::GaborNoiseGeneratorSpec{
                .dotsPerCell = 100,
                .impulseDensity = specDefault.impulseDensity,
                .kernelSpatialExtent = specDefault.kernelSpatialExtent,
                .kernelOscillationFrequency = specDefault.kernelOscillationFrequency
            },
            .scale = 1.0F,
        });
    });
    auto* addSimplexAction = addGeneratorMenu->addAction(QStringLiteral("Simplex"));
    connect(addSimplexAction, &QAction::triggered, this, [this] {
        pipelineModel_->appendGenerator(wgen::GeneratorSpec{
            .kind = wgen::GeneratorKind::SimplexNoise,
            .config = wgen::SimplexNoiseGeneratorSpec{
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
                .numPoints = 1,
                .p = 2.0F
            },
            .scale = 1.0F,
        });
    });
    auto* addWaveletAction = addGeneratorMenu->addAction(QStringLiteral("Wavelet"));
    connect(addWaveletAction, &QAction::triggered, this, [this] {
        pipelineModel_->appendGenerator(wgen::GeneratorSpec{
            .kind = wgen::GeneratorKind::WaveletNoise,
            .config = wgen::WaveletNoiseGeneratorSpec{
                .kWidth = 5,
                .kheight = 5,
                .waveletParams = glm::vec4{0.25f, 0.5f, 0.25f, 0.014231234F}
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
    connect(ui_->clearPipelineButton, &QPushButton::clicked, this, [this] {
        clearPipeline();
    });

    auto openGeneratorSettings = [this](const QModelIndex& index) {
        const wgen::GeneratorSpec* spec = pipelineModel_->generatorAt(index.row());
        if (spec == nullptr) {
            return;
        }

        GeneratorSettingsDialog dialog{*spec, this};
        if (dialog.exec() == QDialog::Accepted) {
            pipelineModel_->updateGenerator(index.row(), dialog.generatorSpec());
        }
    };

    connect(ui_->listView, &QListView::doubleClicked, this, [openGeneratorSettings](const QModelIndex& index) {
        openGeneratorSettings(index);
    });

    ui_->listView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui_->listView, &QListView::customContextMenuRequested, this, [this, openGeneratorSettings](const QPoint& pos) {
        const QModelIndex index = ui_->listView->indexAt(pos);
        if (!index.isValid() || pipelineModel_->generatorAt(index.row()) == nullptr) {
            return;
        }

        ui_->listView->setCurrentIndex(index);

        QMenu menu{this};
        QAction* openSettingsAction = menu.addAction(QStringLiteral("Open generator settings"));
        QAction* removeGeneratorAction = menu.addAction(QStringLiteral("Remove generator"));
        QAction* duplicateGeneratorAction = menu.addAction(QStringLiteral("Duplicate"));

        const QAction* selectedAction = menu.exec(ui_->listView->viewport()->mapToGlobal(pos));
        if (selectedAction == openSettingsAction) {
            openGeneratorSettings(index);
            return;
        }
        if (selectedAction == removeGeneratorAction) {
            pipelineModel_->removeGenerator(index.row());
        }
        if (selectedAction == duplicateGeneratorAction) {
            auto gen = pipelineModel_->generatorAt(index.row());
            pipelineModel_->appendGenerator(wgen::GeneratorSpec{
                .kind = gen->kind,
                .config = gen->config,
                .scale = gen->scale,
                .computeMethod = gen->computeMethod,
                .octaveSettings = gen->octaveSettings
            });
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


    auto openAppSettings = [this]() {
        wgen::WindowConfig config = callbacks_.getConfig().windowConfig;

        AppSettingsDialog dialog{config, this};
        if (dialog.exec() == QDialog::Accepted) {
            config = dialog.appSettings();
            callbacks_.configChanged(config);
        }
    };

    connect(ui_->appSettingsButton, &QPushButton::clicked, this, [openAppSettings]{
        openAppSettings();
    });


    auto openSaveDialog = [this]() {
        SaveTerrainDialog dialog{this};
        if (dialog.exec() == QDialog::Accepted) {
            ExportConfig cfg = dialog.getConfig();
            callbacks_.exportWithConfig(cfg);
        }
    };
    connect(ui_->saveButton, &QPushButton::clicked, this, [openSaveDialog]{
        openSaveDialog();
    });
}

TerrainControlsWidget::~TerrainControlsWidget() = default;

void TerrainControlsWidget::clearPipeline() {
    pipelineModel_->clear();
}

QWidget& TerrainControlsWidget::vulkanWidget() {
    return *ui_->vulkanWidget;
}

} // namespace lve
