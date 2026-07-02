#include "generator_settings_dialog.hpp"

#include "ui_generator_settings_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <algorithm>
#include <limits>
#include <utility>

namespace lve {

namespace {

int computeMethodIndex(wgen::TerrainComputeMethod computeMethod) {
    switch (computeMethod) {
        case wgen::TerrainComputeMethod::Cpu:
            return 0;
        case wgen::TerrainComputeMethod::VulkanCompute:
            return 1;
    }

    return 0;
}

wgen::TerrainComputeMethod computeMethodFromIndex(int index) {
    switch (index) {
        case 1:
            return wgen::TerrainComputeMethod::VulkanCompute;
        case 0:
        default:
            return wgen::TerrainComputeMethod::Cpu;
    }
}

} // namespace

GeneratorSettingsDialog::GeneratorSettingsDialog(wgen::GeneratorSpec spec, QWidget* parent)
    : QDialog{parent}, ui_{std::make_unique<Ui::GeneratorSettingsDialog>()}, spec_{std::move(spec)} {
    ui_->setupUi(this);
    setWindowTitle(generatorName(spec_.kind) + QStringLiteral(" Settings"));

    ui_->generatorValueLabel->setText(generatorName(spec_.kind));

    if (auto* perlin = std::get_if<wgen::PerlinNoiseGeneratorSpec>(&spec_.config)) {
        ui_->dotsPerCellSpinBox->setRange(2, std::numeric_limits<int>::max());
        ui_->dotsPerCellSpinBox->setValue(static_cast<int>(std::min<std::size_t>(
            perlin->dotsPerCell,
            static_cast<std::size_t>(std::numeric_limits<int>::max()))));
    } else {
        ui_->dotsPerCellSpinBox->setVisible(false);
        if (QWidget* label = ui_->formLayout->labelForField(ui_->dotsPerCellSpinBox)) {
            label->setVisible(false);
        }
    }

    ui_->scaleSpinBox->setValue(spec_.scale);
    ui_->computeMethodComboBox->setCurrentIndex(computeMethodIndex(spec_.computeMethod));
    connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

GeneratorSettingsDialog::~GeneratorSettingsDialog() = default;

void GeneratorSettingsDialog::accept() {
    spec_.scale = static_cast<float>(ui_->scaleSpinBox->value());
    spec_.computeMethod = computeMethodFromIndex(ui_->computeMethodComboBox->currentIndex());

    if (std::holds_alternative<wgen::PerlinNoiseGeneratorSpec>(spec_.config)) {
        spec_.config = wgen::PerlinNoiseGeneratorSpec{
            .dotsPerCell = static_cast<std::size_t>(ui_->dotsPerCellSpinBox->value()),
        };
    }

    QDialog::accept();
}

QString GeneratorSettingsDialog::generatorName(wgen::GeneratorKind kind) {
    switch (kind) {
        case wgen::GeneratorKind::PerlinNoise:
            return QStringLiteral("Perlin");
        case wgen::GeneratorKind::ValueNoise:
            return QStringLiteral("Value noise");
    }

    return QStringLiteral("Generator");
}

} // namespace lve
