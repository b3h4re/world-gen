#include "qt_window_backend.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QString>
#include <QtGui/QGuiApplication>

#include <array>
#include <stdexcept>
#include <utility>

namespace lve {

class QtWindowBackend::OwnedGuiApplication {
public:
    OwnedGuiApplication() : argc_{1}, argvStorage_{"world_gen_qt_backend"}, argv_{argvStorage_.data(), nullptr} {
        application_ = std::make_unique<QGuiApplication>(argc_, argv_.data());
    }

private:
    int argc_;
    std::string argvStorage_;
    std::array<char*, 2> argv_;
    std::unique_ptr<QGuiApplication> application_;
};

QtWindowBackend::QtWindowBackend(int w, int h, std::string name)
    : ownedApplication_{ensureGuiApplication()} {
    setSurfaceType(QSurface::VulkanSurface);
    resize(w, h);
    setTitle(QString::fromStdString(name));
}

QtWindowBackend::~QtWindowBackend() {
    destroy();
}

bool QtWindowBackend::shouldClose() const {
    return closeRequested_;
}

void QtWindowBackend::requestClose() {
    closeRequested_ = true;
    close();
}

VkExtent2D QtWindowBackend::getExtent() const {
    return {
        static_cast<uint32_t>(width()),
        static_cast<uint32_t>(height()),
    };
}

bool QtWindowBackend::wasWindowResized() const {
    return frameBufferResized_;
}

void QtWindowBackend::resetWindowResizedFlag() {
    frameBufferResized_ = false;
}

void QtWindowBackend::waitEvents() {
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
}

void QtWindowBackend::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) {
    vulkanInstance_.setVkInstance(instance);
    if (!vulkanInstance_.create()) {
        throw std::runtime_error("failed to initialize Qt Vulkan instance wrapper");
    }

    setVulkanInstance(&vulkanInstance_);
    create();
    show();

    *surface = QVulkanInstance::surfaceForWindow(this);
    if (*surface == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to create Qt Vulkan window surface");
    }
}

std::vector<const char*> QtWindowBackend::getRequiredInstanceExtensions() const {
    return platformSurfaceExtensions();
}

bool QtWindowBackend::event(QEvent* event) {
    switch (event->type()) {
        case QEvent::Close:
            closeRequested_ = true;
            break;
        case QEvent::Resize:
            frameBufferResized_ = true;
            break;
        default:
            break;
    }

    return QWindow::event(event);
}

std::unique_ptr<QtWindowBackend::OwnedGuiApplication> QtWindowBackend::ensureGuiApplication() {
    if (QCoreApplication::instance() != nullptr) {
        return nullptr;
    }

    return std::make_unique<OwnedGuiApplication>();
}

std::vector<const char*> QtWindowBackend::platformSurfaceExtensions() {
    std::vector<const char*> extensions{
        VK_KHR_SURFACE_EXTENSION_NAME,
    };

#if defined(Q_OS_WIN)
    extensions.push_back("VK_KHR_win32_surface");
#elif defined(Q_OS_MACOS)
    extensions.push_back("VK_EXT_metal_surface");
#elif defined(Q_OS_ANDROID)
    extensions.push_back("VK_KHR_android_surface");
#elif defined(Q_OS_UNIX)
    const QString platformName = QGuiApplication::platformName().toLower();
    if (platformName.contains("wayland")) {
        extensions.push_back("VK_KHR_wayland_surface");
    } else {
        extensions.push_back("VK_KHR_xcb_surface");
    }
#endif

    return extensions;
}

} // namespace lve
