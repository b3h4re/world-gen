#include "planet_generator_settings_dialog.hpp"

#include "terrain_compute_method_ui.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QVariant>
#include <QVBoxLayout>

#include <limits>
#include <stdexcept>
#include <utility>

namespace lve {

PlanetGeneratorSettingsDialog::PlanetGeneratorSettingsDialog(wgen::Generator3dSpec spec, QWidget* parent)
    : QDialog{parent}, spec_{std::move(spec)} {
    setWindowTitle(QStringLiteral("3D Perlin Settings"));

    auto* layout = new QVBoxLayout{this};
    auto* form = new QFormLayout{};
    layout->addLayout(form);

    computeMethodComboBox_ = new QComboBox{this};
    computeMethodComboBox_->addItem(QStringLiteral("CPU"));
    computeMethodComboBox_->addItem(QStringLiteral("Vulkan compute"));
    const bool supportsVulkan = wgen::generator3dSupportsVulkanCompute(spec_.kind);
    computeMethodComboBox_->setItemData(
        computeMethodIndex(wgen::TerrainComputeMethod::VulkanCompute),
        supportsVulkan ? QVariant(Qt::ItemIsSelectable | Qt::ItemIsEnabled) : QVariant(Qt::NoItemFlags),
        Qt::UserRole - 1);
    if (!supportsVulkan && spec_.computeMethod == wgen::TerrainComputeMethod::VulkanCompute) {
        spec_.computeMethod = wgen::TerrainComputeMethod::Cpu;
    }
    computeMethodComboBox_->setCurrentIndex(computeMethodIndex(spec_.computeMethod));
    form->addRow(QStringLiteral("Compute"), computeMethodComboBox_);

    cellSizeSpinBox_ = new QDoubleSpinBox{this};
    cellSizeSpinBox_->setDecimals(4);
    cellSizeSpinBox_->setRange(0.0001, std::numeric_limits<double>::max());
    cellSizeSpinBox_->setSingleStep(0.1);
    if (const auto* perlin = std::get_if<wgen::PerlinNoise3dGeneratorSpec>(&spec_.config)) {
        cellSizeSpinBox_->setValue(perlin->cellSize);
    }
    form->addRow(QStringLiteral("Cell size"), cellSizeSpinBox_);

    scaleSpinBox_ = new QDoubleSpinBox{this};
    scaleSpinBox_->setDecimals(4);
    scaleSpinBox_->setRange(-1000000.0, 1000000.0);
    scaleSpinBox_->setSingleStep(0.1);
    scaleSpinBox_->setValue(spec_.scale);
    form->addRow(QStringLiteral("Scale"), scaleSpinBox_);

    auto* buttonBox = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this};
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}

void PlanetGeneratorSettingsDialog::accept() {
    const float cellSize = static_cast<float>(cellSizeSpinBox_->value());
    if (cellSize <= 0.0F) {
        throw std::invalid_argument("3D Perlin cell size must be positive");
    }

    spec_.config = wgen::PerlinNoise3dGeneratorSpec{.cellSize = cellSize};
    spec_.scale = static_cast<float>(scaleSpinBox_->value());
    spec_.computeMethod = computeMethodFromIndex(computeMethodComboBox_->currentIndex());
    if (!wgen::generator3dSupportsVulkanCompute(spec_.kind) &&
            spec_.computeMethod == wgen::TerrainComputeMethod::VulkanCompute) {
        spec_.computeMethod = wgen::TerrainComputeMethod::Cpu;
    }

    QDialog::accept();
}

} // namespace lve
