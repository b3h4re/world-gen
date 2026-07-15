#pragma once

#include "terrain/generators/3d/generator_spec.hpp"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;

namespace lve {

class PlanetGeneratorSettingsDialog : public QDialog {
public:
    explicit PlanetGeneratorSettingsDialog(wgen::Generator3dSpec spec, QWidget* parent = nullptr);

    wgen::Generator3dSpec generatorSpec() const { return spec_; }

protected:
    void accept() override;

private:
    wgen::Generator3dSpec spec_;
    QComboBox* computeMethodComboBox_{};
    QDoubleSpinBox* cellSizeSpinBox_{};
    QDoubleSpinBox* scaleSpinBox_{};
    QSpinBox* firstVisibleLodSpinBox_{};
    QSpinBox* numOctaveSpinBox_{};
    QDoubleSpinBox* lacunaritySpinBox_{};
    QDoubleSpinBox* persistenceSpinBox_{};
};

} // namespace lve
