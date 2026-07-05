#pragma once

#include "terrain/generators/generator_spec.hpp"

#include <QDialog>
#include <QSpinBox>

#include <memory>


template<class SpinBox>
concept set_visible = requires (SpinBox* s) { s->setVisible(true);s->setVisible(true); };
template<class SpinBox, typename Num>
concept set_range = requires (SpinBox* s, Num n) { s->setRange(n, n); };
template<class SpinBox, typename Num>
concept set_value = requires (SpinBox* s, Num n) { s->setValue(n); };

template<class SpinBox, typename Num>
concept valid_spinbox = set_visible<SpinBox> || set_range<SpinBox, Num> || set_value<SpinBox, Num>;


namespace Ui {
class GeneratorSettingsDialog;
}

namespace lve {

class GeneratorSettingsDialog : public QDialog {
public:
    explicit GeneratorSettingsDialog(wgen::GeneratorSpec spec, QWidget* parent = nullptr);
    ~GeneratorSettingsDialog() override;

    wgen::GeneratorSpec generatorSpec() const { return spec_; }

protected:
    void accept() override;

private:
    static QString generatorName(wgen::GeneratorKind kind);

    std::unique_ptr<Ui::GeneratorSettingsDialog> ui_;
    wgen::GeneratorSpec spec_;

    QWidget* getLabelForField(QWidget *field);

    template<class SpinBox> requires set_visible<SpinBox>
    void enableSpinBox(SpinBox* s) {
        s->setVisible(true);
        if (QWidget* label = getLabelForField(s)) {
            label->setVisible(true);
        }
    }
    template<class SpinBox> requires set_visible<SpinBox>
    void disableSpinBox(SpinBox* s) {
        s->setVisible(false);
        if (QWidget* label = getLabelForField(s)) {
            label->setVisible(false);
        }
    }

    template<class SpinBox, typename Num> requires valid_spinbox<SpinBox, Num>
    void setupSpinBox(SpinBox* s, const Num& min, const Num& max, const Num& val) {
        enableSpinBox(s);
        s->setRange(min, max);
        s->setValue(val);
    }

    void enableDotsPerCell(std::size_t val = 100);
    void enableFrequency(float f = 0.014231234F);
    void enableImpulseDensity(float impulseDensity = 1.0F);
    void enableSpatialExtent(float spatialExtent = 1.5F);
    void enablePower(float p = 2.0F);
    void enableNumPoints(std::size_t numPoints = 1);
    void enableOctaveSettings(const wgen::GeneratorOctaveSettings& settings);
    void disableOctaveSettings();

};

} // namespace lve
