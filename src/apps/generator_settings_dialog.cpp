#include "generator_settings_dialog.hpp"

#include "terrain/generators/generator_compute_capabilities.hpp"
#include "terrain_compute_method_ui.hpp"
#include "ui_generator_settings_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <algorithm>
#include <limits>
#include <utility>

namespace lve {

QWidget* GeneratorSettingsDialog::getLabelForField(QWidget *field) {
    return ui_->formLayout->labelForField(field);
}

void GeneratorSettingsDialog::enableDotsPerCell(std::size_t val) {
    setupSpinBox(
        ui_->dotsPerCellSpinBox,
        2,
        std::numeric_limits<int>::max(),
        static_cast<int>(std::min<std::size_t>(
            val,
            static_cast<std::size_t>(std::numeric_limits<int>::max())))
    );
}

void GeneratorSettingsDialog::enableFrequency(float f) {
    setupSpinBox(
        ui_->frequencySpinBox,
        0.0F,
        std::numeric_limits<float>::max(),
        f
    );
}

void GeneratorSettingsDialog::enablePower(float p) {
    setupSpinBox(
        ui_->powerSpinBox,
        0.01F,
        std::numeric_limits<float>::max(),
        p
    );
}

void GeneratorSettingsDialog::enableNumPoints(std::size_t numPoints) {
    setupSpinBox(
        ui_->numPointsSpinBox,
        1,
        8,
        static_cast<int>(std::min<std::size_t>(
            numPoints,
            static_cast<std::size_t>(std::numeric_limits<int>::max())))
    );
}

GeneratorSettingsDialog::GeneratorSettingsDialog(wgen::GeneratorSpec spec, QWidget* parent)
    : QDialog{parent}, ui_{std::make_unique<Ui::GeneratorSettingsDialog>()}, spec_{std::move(spec)} {
    ui_->setupUi(this);
    setWindowTitle(generatorName(spec_.kind) + QStringLiteral(" Settings"));

    ui_->generatorValueLabel->setText(generatorName(spec_.kind));

    disableSpinBox(ui_->dotsPerCellSpinBox);
    disableSpinBox(ui_->frequencySpinBox);
    disableSpinBox(ui_->powerSpinBox);
    disableSpinBox(ui_->numPointsSpinBox);

    if (auto* perlin = std::get_if<wgen::PerlinNoiseGeneratorSpec>(&spec_.config)) {
        enableDotsPerCell(perlin->dotsPerCell);
    }
    if (auto* worley = std::get_if<wgen::WorleyNoiseGeneratorSpec>(&spec_.config)) {
        enableDotsPerCell(worley->dotsPerCell);
        enablePower(worley->p);
        enableNumPoints(worley->numPoints);
    }
    if (auto* simplex = std::get_if<wgen::SimplexNoiseGeneratorSpec>(&spec_.config)) {
        enableDotsPerCell(simplex->dotsPerCell);
    }

    ui_->scaleSpinBox->setValue(spec_.scale);
    if (!wgen::generatorSupportsComputeMethod(spec_.kind, spec_.computeMethod)) {
        spec_.computeMethod = wgen::TerrainComputeMethod::Cpu;
    }
    bool supported = wgen::generatorSupportsComputeMethod(spec_.kind, wgen::TerrainComputeMethod::VulkanCompute);
    ui_->computeMethodComboBox->setItemData(
        computeMethodIndex(wgen::TerrainComputeMethod::VulkanCompute),
        supported ? QVariant(Qt::ItemIsSelectable | Qt::ItemIsEnabled) : QVariant(Qt::NoItemFlags),
        Qt::UserRole - 1);
    ui_->computeMethodComboBox->setCurrentIndex(computeMethodIndex(spec_.computeMethod));
    connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

GeneratorSettingsDialog::~GeneratorSettingsDialog() = default;

void GeneratorSettingsDialog::accept() {
    spec_.scale = static_cast<float>(ui_->scaleSpinBox->value());
    spec_.computeMethod = computeMethodFromIndex(ui_->computeMethodComboBox->currentIndex());
    if (!wgen::generatorSupportsComputeMethod(spec_.kind, spec_.computeMethod)) {
        spec_.computeMethod = wgen::TerrainComputeMethod::Cpu;
    }

    if (std::holds_alternative<wgen::PerlinNoiseGeneratorSpec>(spec_.config)) {
        spec_.config = wgen::PerlinNoiseGeneratorSpec{
            .dotsPerCell = static_cast<std::size_t>(ui_->dotsPerCellSpinBox->value()),
        };
    }

    if (std::holds_alternative<wgen::WorleyNoiseGeneratorSpec>(spec_.config)) {
        spec_.config = wgen::WorleyNoiseGeneratorSpec{
            .dotsPerCell = static_cast<std::size_t>(ui_->dotsPerCellSpinBox->value()),
            .numPoints = static_cast<std::size_t>(ui_->numPointsSpinBox->value()),
            .p = static_cast<float>(ui_->powerSpinBox->value())
        };
    }

    QDialog::accept();
}

QString GeneratorSettingsDialog::generatorName(wgen::GeneratorKind kind) {
    switch (kind) {
        case wgen::GeneratorKind::PerlinNoise:
            return QStringLiteral("Perlin");
        case wgen::GeneratorKind::SimplexNoise:
            return QStringLiteral("Simplex");
        case wgen::GeneratorKind::WorleyNoise:
            return QStringLiteral("Worley");
        case wgen::GeneratorKind::ValueNoise:
            return QStringLiteral("Value noise");
    }

    return QStringLiteral("Generator");
}

} // namespace lve
