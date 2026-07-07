#include "generator_settings_dialog.hpp"

#include "terrain/generators/2d/generator_compute_capabilities.hpp"
#include "terrain_compute_method_ui.hpp"
#include "terrain/generators/2d/noise/wavelet.hpp"
#include "ui_generator_settings_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <algorithm>
#include <limits>
#include <utility>

namespace lve {

QWidget* GeneratorSettingsDialog::getLabelForField(QWidget *field) {
    if (QWidget* label = ui_->formLayout->labelForField(field)) {
        return label;
    }

    return ui_->octaveFormLayout->labelForField(field);
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


void GeneratorSettingsDialog::enableImpulseDensity(float impulseDensity) {
    setupSpinBox(
        ui_->impulseDensitySpinBox,
        0.0F,
        std::numeric_limits<float>::max(),
        impulseDensity
    );
}

void GeneratorSettingsDialog::enableSpatialExtent(float spatialExtent) {
    setupSpinBox(
        ui_->spatialExtentSpinBox,
        0.01F,
        std::numeric_limits<float>::max(),
        spatialExtent
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
        std::numeric_limits<int>::max(),
        static_cast<int>(std::min<std::size_t>(
            numPoints,
            static_cast<std::size_t>(std::numeric_limits<int>::max())))
    );
}


void GeneratorSettingsDialog::enableFilterParams(float a, float b, float c) {
    wgen::WaveletNoise2d::assertFilterParams(a, b, c);
    float min = std::numeric_limits<float>::min();
    float max = std::numeric_limits<float>::max();
    ui_->separableFilterParamsALabel->setVisible(true);
    ui_->separableFilterParamsBLabel->setVisible(true);
    ui_->separableFilterParamsCLabel->setVisible(true);
    ui_->separableFilterParamsLabel->setVisible(true);
    setupSpinBox(
        ui_->separableFilterParamsASpinBox,
        min, max,
        a
    );
    setupSpinBox(
        ui_->separableFilterParamsBSpinBox,
        min, max,
        b
    );
    setupSpinBox(
        ui_->separableFilterParamsCSpinBox,
        min, max,
        c
    );
}

void GeneratorSettingsDialog::enableKernelSize(std::uint32_t kWidth, std::uint32_t kHeight) {
    ui_->kernelSizeLabel->setVisible(true);
    ui_->kernelWidthLabel->setVisible(true);
    ui_->kernelHeightLabel->setVisible(true);
    setupSpinBox(
        ui_->kernelWidthSpinBox,
        1,
        std::numeric_limits<int>::max(),
        static_cast<int>(std::min<std::size_t>(
            kWidth,
            static_cast<std::size_t>(std::numeric_limits<int>::max())))
    );
    setupSpinBox(
        ui_->kernelHeightSpinBox,
        1,
        std::numeric_limits<int>::max(),
        static_cast<int>(std::min<std::size_t>(
            kHeight,
            static_cast<std::size_t>(std::numeric_limits<int>::max())))
    );
}

void GeneratorSettingsDialog::enableOctaveSettings(const wgen::GeneratorOctaveSettings& settings) {
    ui_->octaveSettingsLine->setVisible(true);
    ui_->octaveSettingsLabel->setVisible(true);
    setupSpinBox(
        ui_->numOctaveSpinBox,
        0,
        std::numeric_limits<int>::max(),
        static_cast<int>(std::min<std::size_t>(
            settings.numOctave,
            static_cast<std::size_t>(std::numeric_limits<int>::max())))
    );
    setupSpinBox(
        ui_->lacunaritySpinBox,
        0.0F,
        std::numeric_limits<float>::max(),
        settings.lacunarity
    );
    setupSpinBox(
        ui_->persistanceSpinBox,
        0.0F,
        std::numeric_limits<float>::max(),
        settings.persistance
    );
}

void GeneratorSettingsDialog::disableFilterParams() {
    ui_->separableFilterParamsALabel->setVisible(false);
    ui_->separableFilterParamsBLabel->setVisible(false);
    ui_->separableFilterParamsCLabel->setVisible(false);
    ui_->separableFilterParamsLabel->setVisible(false);
    disableSpinBox(ui_->separableFilterParamsASpinBox);
    disableSpinBox(ui_->separableFilterParamsBSpinBox);
    disableSpinBox(ui_->separableFilterParamsCSpinBox);
}

void GeneratorSettingsDialog::disableKernelSize() {
    ui_->kernelSizeLabel->setVisible(false);
    ui_->kernelWidthLabel->setVisible(false);
    ui_->kernelHeightLabel->setVisible(false);
    disableSpinBox(ui_->kernelWidthSpinBox);
    disableSpinBox(ui_->kernelHeightSpinBox);
}

void GeneratorSettingsDialog::disableOctaveSettings() {
    ui_->octaveSettingsLine->setVisible(false);
    ui_->octaveSettingsLabel->setVisible(false);
    disableSpinBox(ui_->numOctaveSpinBox);
    disableSpinBox(ui_->lacunaritySpinBox);
    disableSpinBox(ui_->persistanceSpinBox);
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
    disableSpinBox(ui_->impulseDensitySpinBox);
    disableSpinBox(ui_->spatialExtentSpinBox);
    disableFilterParams();
    disableKernelSize();
    disableOctaveSettings();

    if (auto* wavelet = std::get_if<wgen::WaveletNoiseGeneratorSpec>(&spec_.config)) {
        enableFrequency(wavelet->waveletParams.w);
        enableFilterParams(wavelet->waveletParams.x, wavelet->waveletParams.y, wavelet->waveletParams.z);
        enableKernelSize(wavelet->kWidth, wavelet->kheight);
    }
    if (auto* gabor = std::get_if<wgen::GaborNoiseGeneratorSpec>(&spec_.config)) {
        enableDotsPerCell(gabor->dotsPerCell);
        enableFrequency(gabor->kernelOscillationFrequency);
        enableImpulseDensity(gabor->impulseDensity);
        enableSpatialExtent(gabor->kernelSpatialExtent);
    }
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
    if (wgen::generatorSupportsOctaves(spec_.kind)) {
        enableOctaveSettings(spec_.octaveSettings);
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

    connect(ui_->separableFilterParamsASpinBox, &QDoubleSpinBox::valueChanged, this, [this](double) { validateSeparableFilterSpinBox(); });
    connect(ui_->separableFilterParamsBSpinBox, &QDoubleSpinBox::valueChanged, this, [this](double) { validateSeparableFilterSpinBox(); });
    connect(ui_->separableFilterParamsCSpinBox, &QDoubleSpinBox::valueChanged, this, [this](double) { validateSeparableFilterSpinBox(); });
}

GeneratorSettingsDialog::~GeneratorSettingsDialog() = default;

bool GeneratorSettingsDialog::validateSeparableFilterSpinBox() {
    float a = ui_->separableFilterParamsASpinBox->value();
    float b = ui_->separableFilterParamsBSpinBox->value();
    float c = ui_->separableFilterParamsCSpinBox->value();
    bool isValid = true;
    try {
        wgen::WaveletNoise2d::assertFilterParams(a, b, c);
    } catch (const std::invalid_argument& e) {
        isValid = false;
    }

    ui_->separableFilterParamsASpinBox->setStyleSheet(isValid ? "" : "QDoubleSpinBox { border: 1px solid red; }");
    ui_->separableFilterParamsBSpinBox->setStyleSheet(isValid ? "" : "QDoubleSpinBox { border: 1px solid red; }");
    ui_->separableFilterParamsCSpinBox->setStyleSheet(isValid ? "" : "QDoubleSpinBox { border: 1px solid red; }");

    return isValid;
}

void GeneratorSettingsDialog::accept() {
    spec_.scale = static_cast<float>(ui_->scaleSpinBox->value());
    spec_.computeMethod = computeMethodFromIndex(ui_->computeMethodComboBox->currentIndex());
    if (!wgen::generatorSupportsComputeMethod(spec_.kind, spec_.computeMethod)) {
        spec_.computeMethod = wgen::TerrainComputeMethod::Cpu;
    }
    if (wgen::generatorSupportsOctaves(spec_.kind)) {
        spec_.octaveSettings = wgen::GeneratorOctaveSettings{
            .numOctave = static_cast<std::size_t>(ui_->numOctaveSpinBox->value()),
            .lacunarity = static_cast<float>(ui_->lacunaritySpinBox->value()),
            .persistance = static_cast<float>(ui_->persistanceSpinBox->value()),
        };
    }

    if (std::holds_alternative<wgen::PerlinNoiseGeneratorSpec>(spec_.config)) {
        spec_.config = wgen::PerlinNoiseGeneratorSpec{
            .dotsPerCell = static_cast<std::size_t>(ui_->dotsPerCellSpinBox->value()),
        };
    }

    if (std::holds_alternative<wgen::WaveletNoiseGeneratorSpec>(spec_.config)) {
        if (!validateSeparableFilterSpinBox()) {
            return;
        }
        
        spec_.config = wgen::WaveletNoiseGeneratorSpec{
            .kWidth = static_cast<std::uint32_t>(ui_->kernelWidthSpinBox->value()),
            .kheight = static_cast<std::uint32_t>(ui_->kernelHeightSpinBox->value()),
            .waveletParams = glm::vec4{
                ui_->separableFilterParamsASpinBox->value(),
                ui_->separableFilterParamsBSpinBox->value(),
                ui_->separableFilterParamsCSpinBox->value(),
                ui_->frequencySpinBox->value()
            }
        };
    }

    if (std::holds_alternative<wgen::GaborNoiseGeneratorSpec>(spec_.config)) {
        spec_.config = wgen::GaborNoiseGeneratorSpec{
            .dotsPerCell = static_cast<std::size_t>(ui_->dotsPerCellSpinBox->value()),
            .impulseDensity = static_cast<float>(ui_->impulseDensitySpinBox->value()),
            .kernelSpatialExtent = static_cast<float>(ui_->spatialExtentSpinBox->value()),
            .kernelOscillationFrequency = static_cast<float>(ui_->frequencySpinBox->value())
        };
    }

    if (std::holds_alternative<wgen::WorleyNoiseGeneratorSpec>(spec_.config)) {
        spec_.config = wgen::WorleyNoiseGeneratorSpec{
            .dotsPerCell = static_cast<std::size_t>(ui_->dotsPerCellSpinBox->value()),
            .numPoints = static_cast<std::size_t>(ui_->numPointsSpinBox->value()),
            .p = static_cast<float>(ui_->powerSpinBox->value())
        };
    }

    if (std::holds_alternative<wgen::SimplexNoiseGeneratorSpec>(spec_.config)) {
        spec_.config = wgen::SimplexNoiseGeneratorSpec{
            .dotsPerCell = static_cast<std::size_t>(ui_->dotsPerCellSpinBox->value())
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
        case wgen::GeneratorKind::WaveletNoise:
            return QStringLiteral("Wavelet");
        case wgen::GeneratorKind::ValueNoise:
            return QStringLiteral("Value noise");
        case wgen::GeneratorKind::GaborNoise:
            return QStringLiteral("Gabor");
    }

    return QStringLiteral("Generator");
}

} // namespace lve
