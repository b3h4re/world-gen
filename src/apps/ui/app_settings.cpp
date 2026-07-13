#include "app_settings.hpp"

#include "ui_app_settings.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QSlider>

#include <algorithm>
#include <utility>

namespace lve {

AppSettingsDialog::AppSettingsDialog(wgen::WindowConfig settings, QWidget* parent)
    : QDialog{parent},
      ui_{std::make_unique<Ui::Form>()},
      settings_{std::move(settings)} {
    ui_->setupUi(this);

    setWindowTitle(QStringLiteral("App Settings"));

    ui_->generatorLabel->setText(QStringLiteral("Settings"));

    ui_->presentModeComboBox->setCurrentIndex(presentModeIndex(settings_.present_mode));

    // TODO Move the FPS limit range to an application settings/capabilities constant.
    // 0 is currently reserved for "uncapped".
    ui_->fpsLimitSlider->setRange(0, 240);
    ui_->fpsLimitSlider->setSingleStep(1);
    ui_->fpsLimitSlider->setPageStep(10);
    ui_->fpsLimitSlider->setTickInterval(30);
    ui_->fpsLimitSlider->setTickPosition(QSlider::TicksBelow);
    ui_->fpsLimitSlider->setValue(static_cast<int>(std::min<int>(
        settings_.fps_max,
        240u
    )));

    updateSummaryLabel();

    connect(ui_->presentModeComboBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        settings_.present_mode = presentModeFromIndex(index);
        updateSummaryLabel();
    });

    connect(ui_->fpsLimitSlider, &QSlider::valueChanged, this, [this](int value) {
        settings_.fps_max = static_cast<std::uint32_t>(std::max(0, value));
        updateSummaryLabel();
    });

    connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

AppSettingsDialog::~AppSettingsDialog() = default;

void AppSettingsDialog::accept() {
    settings_.present_mode = presentModeFromIndex(ui_->presentModeComboBox->currentIndex());
    settings_.fps_max = static_cast<std::uint32_t>(std::max(0, ui_->fpsLimitSlider->value()));

    // TODO Apply settings to the renderer/application after the dialog returns Accepted.
    // For Vulkan present mode changes, recreate the swapchain at a safe point.
    // For FPS limit changes, update the application's frame limiter.

    QDialog::accept();
}

QString AppSettingsDialog::presentModeName(PresentMode mode) {
    switch (mode) {
        case PresentMode::VSync:
            return QStringLiteral("VSync");
        case PresentMode::LowLatency:
            return QStringLiteral("Mailbox");
        case PresentMode::Immediate:
            return QStringLiteral("Immediate");
    }

    return QStringLiteral("VSync");
}

int AppSettingsDialog::presentModeIndex(PresentMode mode) {
    switch (mode) {
        case PresentMode::VSync:
            return 0;
        case PresentMode::LowLatency:
            return 1;
        case PresentMode::Immediate:
            return 2;
    }

    return 0;
}

PresentMode AppSettingsDialog::presentModeFromIndex(int index) {
    switch (index) {
        case 0:
            return PresentMode::VSync;
        case 1:
            return PresentMode::LowLatency;
        case 2:
            return PresentMode::Immediate;
        default:
            return PresentMode::VSync;
    }
}

void AppSettingsDialog::updateSummaryLabel() {
    const QString fpsText = settings_.fps_max == 0
        ? QStringLiteral("uncapped")
        : QStringLiteral("%1 FPS").arg(settings_.fps_max);

    ui_->generatorValueLabel->setText(
        QStringLiteral("%1, %2").arg(presentModeName(settings_.present_mode), fpsText)
    );
}

} // namespace lve
