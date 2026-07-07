#include "planet_generator_settings_dialog.hpp"

#include "terrain_compute_method_ui.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSpinBox>
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

    if (wgen::generator3dSupportsOctaves(spec_.kind)) {
        numOctaveSpinBox_ = new QSpinBox{this};
        numOctaveSpinBox_->setRange(0, std::numeric_limits<int>::max());
        numOctaveSpinBox_->setValue(static_cast<int>(std::min<std::size_t>(
            spec_.octaveSettings.numOctave,
            static_cast<std::size_t>(std::numeric_limits<int>::max()))));
        form->addRow(QStringLiteral("Octave"), numOctaveSpinBox_);

        lacunaritySpinBox_ = new QDoubleSpinBox{this};
        lacunaritySpinBox_->setDecimals(4);
        lacunaritySpinBox_->setRange(0.0001, std::numeric_limits<double>::max());
        lacunaritySpinBox_->setSingleStep(0.1);
        lacunaritySpinBox_->setValue(spec_.octaveSettings.lacunarity);
        form->addRow(QStringLiteral("Lacunarity"), lacunaritySpinBox_);

        persistenceSpinBox_ = new QDoubleSpinBox{this};
        persistenceSpinBox_->setDecimals(4);
        persistenceSpinBox_->setRange(0.0, std::numeric_limits<double>::max());
        persistenceSpinBox_->setSingleStep(0.1);
        persistenceSpinBox_->setValue(spec_.octaveSettings.persistance);
        form->addRow(QStringLiteral("Persistence"), persistenceSpinBox_);
    }

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
    if (wgen::generator3dSupportsOctaves(spec_.kind)) {
        spec_.octaveSettings = wgen::GeneratorOctaveSettings{
            .numOctave = static_cast<std::size_t>(numOctaveSpinBox_->value()),
            .lacunarity = static_cast<float>(lacunaritySpinBox_->value()),
            .persistance = static_cast<float>(persistenceSpinBox_->value()),
        };
    }

    QDialog::accept();
}

} // namespace lve
