#include "terrain_controls_widget.hpp"

#include "generator_settings_dialog.hpp"
#include "planet_generator_settings_dialog.hpp"
#include "app_settings.hpp"
#include "save_terrain.hpp"
#include "files/exporter.hpp"
#include "planet_pipeline_list_model.hpp"
#include "terrain_pipeline_list_model.hpp"
#include "terrain/planet/planet_lod_selector.hpp"
#include "ui_terrain_controls_widget.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QListView>
#include <QMenu>
#include <QPoint>
#include <QMessageBox>
#include <QTabWidget>
#include <QSpinBox>

#include <algorithm>
#include <limits>
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

    wgen::Generator3dPipelineSpec initialPlanetPipeline;
    if (callbacks_.currentPlanetPipeline) {
        initialPlanetPipeline = callbacks_.currentPlanetPipeline();
    } else {
        initialPlanetPipeline = wgen::Generator3dPipelineSpec{
            wgen::Generator3dSpec{
                .kind = wgen::Generator3dKind::PerlinNoise,
                .config = wgen::PerlinNoise3dGeneratorSpec{},
            },
        };
    }

    planetPipelineModel_ = std::make_unique<PlanetPipelineListModel>(std::move(initialPlanetPipeline));
    ui_->planetListView->setModel(planetPipelineModel_.get());

    const wgen::AppConfig initialAppConfig = callbacks_.getConfig
        ? callbacks_.getConfig()
        : wgen::AppConfig{};
    const wgen::PlanetConfig& initialPlanetConfig = initialAppConfig.planetConfig;
    const bool automaticResolution = initialPlanetConfig.resolution == 0;
    const std::size_t displayedResolution = automaticResolution
        ? std::min(initialAppConfig.terrainConfig.width, initialAppConfig.terrainConfig.height)
        : initialPlanetConfig.resolution;
    ui_->planetResolutionSpinBox->setMaximum(std::numeric_limits<int>::max());
    ui_->planetRadiusSpinBox->setMaximum(std::numeric_limits<float>::max());
    ui_->planetRadiusSpinBox->setSuffix(QStringLiteral(" m"));
    ui_->automaticPlanetResolutionCheckBox->setChecked(automaticResolution);
    ui_->planetResolutionSpinBox->setEnabled(!automaticResolution);
    ui_->planetResolutionSpinBox->setValue(static_cast<int>(std::min<std::size_t>(
        displayedResolution,
        static_cast<std::size_t>(std::numeric_limits<int>::max()))));
    ui_->planetRadiusSpinBox->setValue(initialPlanetConfig.radius);

    auto notifyPlanetShapeChanged = [this] {
        if (callbacks_.planetShapeChanged) {
            const std::size_t resolution = ui_->automaticPlanetResolutionCheckBox->isChecked()
                ? 0
                : static_cast<std::size_t>(ui_->planetResolutionSpinBox->value());
            callbacks_.planetShapeChanged(resolution, static_cast<float>(ui_->planetRadiusSpinBox->value()));
        }
    };
    connect(
        ui_->automaticPlanetResolutionCheckBox,
        &QCheckBox::toggled,
        this,
        [this, notifyPlanetShapeChanged](bool checked) {
            ui_->planetResolutionSpinBox->setEnabled(!checked);
            notifyPlanetShapeChanged();
        });
    connect(ui_->planetResolutionSpinBox, &QSpinBox::valueChanged, this, [notifyPlanetShapeChanged](int) {
        notifyPlanetShapeChanged();
    });
    connect(ui_->planetRadiusSpinBox, &QDoubleSpinBox::valueChanged, this, [notifyPlanetShapeChanged](double) {
        notifyPlanetShapeChanged();
    });
    ui_->maximumPlanetPatchLevelSpinBox->setMaximum(wgen::MAX_PLANET_PATCH_LEVEL);
    ui_->maximumPlanetPatchLevelSpinBox->setValue(wgen::DEFAULT_PLANET_MAX_LOD);
    connect(ui_->maximumPlanetPatchLevelSpinBox, &QSpinBox::valueChanged, this, [this](int level) {
        if (callbacks_.maximumPlanetPatchLevelChanged) {
            callbacks_.maximumPlanetPatchLevelChanged(static_cast<std::uint8_t>(level));
        }
    });
    ui_->planetLodTransitionTimeScaleSpinBox->setMaximum(
        std::numeric_limits<double>::max());
    ui_->planetLodTransitionTimeScaleSpinBox->setValue(
        initialPlanetConfig.lodTransitionTimeScale);
    connect(
        ui_->planetLodTransitionTimeScaleSpinBox,
        &QDoubleSpinBox::valueChanged,
        this,
        [this](double timeScale) {
            if (callbacks_.planetLodTransitionTimeScaleChanged) {
                callbacks_.planetLodTransitionTimeScaleChanged(timeScale);
            }
        });

    ui_->geometryBlendValueSpinBox->setEnabled(false);
    const auto notifyGeometryBlendDebugChanged = [this] {
        if (callbacks_.planetGeometryBlendDebugChanged) {
            callbacks_.planetGeometryBlendDebugChanged(
                ui_->freezeGeometryBlendCheckBox->isChecked(),
                ui_->geometryBlendValueSpinBox->value());
        }
    };
    connect(
        ui_->freezeGeometryBlendCheckBox,
        &QCheckBox::toggled,
        this,
        [this, notifyGeometryBlendDebugChanged](bool frozen) {
            ui_->geometryBlendValueSpinBox->setEnabled(frozen);
            notifyGeometryBlendDebugChanged();
        });
    connect(
        ui_->geometryBlendValueSpinBox,
        &QDoubleSpinBox::valueChanged,
        this,
        [notifyGeometryBlendDebugChanged](double) {
            notifyGeometryBlendDebugChanged();
        });

    ui_->navigationBlendValueSpinBox->setEnabled(false);
    const auto notifyNavigationBlendDebugChanged = [this] {
        if (callbacks_.planetNavigationBlendDebugChanged) {
            callbacks_.planetNavigationBlendDebugChanged(
                ui_->freezeNavigationBlendCheckBox->isChecked(),
                ui_->navigationBlendValueSpinBox->value());
        }
    };
    connect(
        ui_->freezeNavigationBlendCheckBox,
        &QCheckBox::toggled,
        this,
        [this, notifyNavigationBlendDebugChanged](bool frozen) {
            ui_->navigationBlendValueSpinBox->setEnabled(frozen);
            notifyNavigationBlendDebugChanged();
        });
    connect(
        ui_->navigationBlendValueSpinBox,
        &QDoubleSpinBox::valueChanged,
        this,
        [notifyNavigationBlendDebugChanged](double) {
            notifyNavigationBlendDebugChanged();
        });

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

    auto notifyPlanetPipelineChanged = [this] {
        if (callbacks_.planetPipelineChanged) {
            callbacks_.planetPipelineChanged(planetPipelineModel_->pipeline());
        }
    };
    connect(planetPipelineModel_.get(), &QAbstractItemModel::modelReset, this, notifyPlanetPipelineChanged);
    connect(planetPipelineModel_.get(), &QAbstractItemModel::rowsInserted, this, notifyPlanetPipelineChanged);
    connect(planetPipelineModel_.get(), &QAbstractItemModel::rowsRemoved, this, notifyPlanetPipelineChanged);
    connect(
        planetPipelineModel_.get(),
        &QAbstractItemModel::dataChanged,
        this,
        [notifyPlanetPipelineChanged](const QModelIndex&, const QModelIndex&, const QList<int>&) {
            notifyPlanetPipelineChanged();
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

    auto* addPlanetGeneratorMenu = new QMenu{ui_->addPlanetGeneratorButton};
    ui_->addPlanetGeneratorButton->setMenu(addPlanetGeneratorMenu);
    auto* addPlanetPerlinAction = addPlanetGeneratorMenu->addAction(QStringLiteral("3D Perlin"));
    connect(addPlanetPerlinAction, &QAction::triggered, this, [this] {
        planetPipelineModel_->appendGenerator(wgen::Generator3dSpec{
            .kind = wgen::Generator3dKind::PerlinNoise,
            .config = wgen::PerlinNoise3dGeneratorSpec{},
            .scale = 1.0F,
        });
    });
    connect(ui_->clearPlanetPipelineButton, &QPushButton::clicked, this, [this] {
        clearPlanetPipeline();
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

    auto openPlanetGeneratorSettings = [this](const QModelIndex& index) {
        const wgen::Generator3dSpec* spec = planetPipelineModel_->generatorAt(index.row());
        if (spec == nullptr) {
            return;
        }

        PlanetGeneratorSettingsDialog dialog{*spec, this};
        if (dialog.exec() == QDialog::Accepted) {
            planetPipelineModel_->updateGenerator(index.row(), dialog.generatorSpec());
        }
    };

    connect(ui_->planetListView, &QListView::doubleClicked, this, [openPlanetGeneratorSettings](const QModelIndex& index) {
        openPlanetGeneratorSettings(index);
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

    ui_->planetListView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui_->planetListView, &QListView::customContextMenuRequested, this, [this, openPlanetGeneratorSettings](const QPoint& pos) {
        const QModelIndex index = ui_->planetListView->indexAt(pos);
        if (!index.isValid() || planetPipelineModel_->generatorAt(index.row()) == nullptr) {
            return;
        }

        ui_->planetListView->setCurrentIndex(index);

        QMenu menu{this};
        QAction* openSettingsAction = menu.addAction(QStringLiteral("Open generator settings"));
        QAction* removeGeneratorAction = menu.addAction(QStringLiteral("Remove generator"));
        QAction* duplicateGeneratorAction = menu.addAction(QStringLiteral("Duplicate"));

        const QAction* selectedAction = menu.exec(ui_->planetListView->viewport()->mapToGlobal(pos));
        if (selectedAction == openSettingsAction) {
            openPlanetGeneratorSettings(index);
            return;
        }
        if (selectedAction == removeGeneratorAction) {
            planetPipelineModel_->removeGenerator(index.row());
        }
        if (selectedAction == duplicateGeneratorAction) {
            auto gen = planetPipelineModel_->generatorAt(index.row());
            planetPipelineModel_->appendGenerator(wgen::Generator3dSpec{
                .kind = gen->kind,
                .config = gen->config,
                .scale = gen->scale,
                .computeMethod = gen->computeMethod,
                .octaveSettings = gen->octaveSettings,
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
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        ExportConfig cfg = dialog.getConfig();
        try {
            callbacks_.exportTerrain(cfg);
            QMessageBox::information(
                this,
                tr("Terrain exported"),
                tr("Terrain was saved successfully.")
            );
        } catch (const std::runtime_error& e) {
            QMessageBox::critical(
                this,
                tr("Export failed"),
                tr("Could not save terrain:\n%1").arg(QString::fromUtf8(e.what()))
            );
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

void TerrainControlsWidget::clearPlanetPipeline() {
    planetPipelineModel_->clear();
}

QWidget& TerrainControlsWidget::vulkanWidget() {
    return *ui_->vulkanWidget;
}

} // namespace lve
