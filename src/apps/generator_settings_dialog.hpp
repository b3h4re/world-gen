#pragma once

#include "terrain/generators/generator_spec.hpp"

#include <QDialog>

#include <memory>

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
};

} // namespace lve
