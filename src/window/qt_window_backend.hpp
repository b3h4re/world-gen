#pragma once

#include "window/window_surface.hpp"

#include <QtCore/QObject>
#include <QtGui/QVulkanInstance>

#include <memory>
#include <string>
#include <vector>

class QEvent;
class QWindow;

namespace lve {

class QtWindowBackend : public QObject, public WindowSurface {
public:
    QtWindowBackend(int w, int h, std::string name);
    ~QtWindowBackend() override;

    QtWindowBackend(const QtWindowBackend&) = delete;
    QtWindowBackend& operator=(const QtWindowBackend&) = delete;

    bool shouldClose() const override;
    void requestClose() override;
    VkExtent2D getExtent() const override;
    bool wasWindowResized() const override;
    void resetWindowResizedFlag() override;
    void pollEvents() override;
    void waitEvents() override;
    void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) override;
    std::vector<const char*> getRequiredInstanceExtensions() const override;

    QWindow& qWindow();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    class OwnedGuiApplication;

    static std::unique_ptr<OwnedGuiApplication> ensureGuiApplication();
    static std::vector<const char*> platformSurfaceExtensions();

    std::unique_ptr<OwnedGuiApplication> ownedApplication_;
    std::unique_ptr<QWindow> window_;
    QVulkanInstance vulkanInstance_{};
    bool closeRequested_{false};
    bool frameBufferResized_{false};
};

} // namespace lve
