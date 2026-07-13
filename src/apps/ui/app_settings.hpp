#pragma once

#include "config/app_config.hpp"
#include "swap_chain/lve_swap_chain.hpp"


#include <QDialog>

#include <memory>

namespace Ui {
class Form;
}

namespace lve {

class AppSettingsDialog : public QDialog {
public:
    explicit AppSettingsDialog(wgen::WindowConfig settings, QWidget* parent = nullptr);
    ~AppSettingsDialog() override;

    wgen::WindowConfig appSettings() const { return settings_; }

protected:
    void accept() override;

private:
    static QString presentModeName(PresentMode mode);
    static int presentModeIndex(PresentMode mode);
    static PresentMode presentModeFromIndex(int index);

    void updateSummaryLabel();

    std::unique_ptr<Ui::Form> ui_;
    wgen::WindowConfig settings_;
};

} // namespace lve
