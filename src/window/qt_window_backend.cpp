#include "qt_window_backend.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QString>
#include <QtGui/QWindow>
#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <array>
#include <stdexcept>
#include <utility>

namespace lve {

class QtWindowBackend::OwnedGuiApplication {
public:
    OwnedGuiApplication() : argc_{1}, argvStorage_{"world_gen_qt_backend"}, argv_{argvStorage_.data(), nullptr} {
        application_ = std::make_unique<QApplication>(argc_, argv_.data());
    }

private:
    int argc_;
    std::string argvStorage_;
    std::array<char*, 2> argv_;
    std::unique_ptr<QApplication> application_;
};

QtWindowBackend::QtWindowBackend(int w, int h, std::string name)
    : ownedApplication_{ensureGuiApplication()},
      rootWidget_{std::make_unique<QWidget>()},
      window_{std::make_unique<QWindow>()} {
    window_->setSurfaceType(QSurface::VulkanSurface);
    window_->resize(w, h);
    window_->setTitle(QString::fromStdString(name));
    window_->installEventFilter(this);

    rootWidget_->setWindowTitle(QString::fromStdString(name));
    rootWidget_->resize(w, h);
    rootWidget_->installEventFilter(this);

    auto* layout = new QVBoxLayout(rootWidget_.get());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    controlsWidget_ = new QWidget{rootWidget_.get()};
    controlsWidget_->setObjectName("terrainControls");
    controlsWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(controlsWidget_);
}

QtWindowBackend::~QtWindowBackend() {
    if (rootWidget_) {
        rootWidget_->removeEventFilter(this);
    }
    if (window_) {
        window_->removeEventFilter(this);
        window_->destroy();
    }
}

bool QtWindowBackend::shouldClose() const {
    return closeRequested_;
}

void QtWindowBackend::requestClose() {
    closeRequested_ = true;
    rootWidget_->close();
}

VkExtent2D QtWindowBackend::getExtent() const {
    return {
        static_cast<uint32_t>(window_->width()),
        static_cast<uint32_t>(window_->height()),
    };
}

bool QtWindowBackend::wasWindowResized() const {
    return frameBufferResized_;
}

void QtWindowBackend::resetWindowResizedFlag() {
    frameBufferResized_ = false;
}

void QtWindowBackend::pollEvents() {
    QCoreApplication::processEvents();
}

void QtWindowBackend::waitEvents() {
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
}

void QtWindowBackend::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) {
    vulkanInstance_.setVkInstance(instance);
    if (!vulkanInstance_.create()) {
        throw std::runtime_error("failed to initialize Qt Vulkan instance wrapper");
    }

    window_->setVulkanInstance(&vulkanInstance_);

    if (windowContainer_ == nullptr) {
        windowContainer_ = QWidget::createWindowContainer(window_.get(), rootWidget_.get());
        windowContainer_->setFocusPolicy(Qt::StrongFocus);
        windowContainer_->setMouseTracking(true);
        windowContainer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        rootWidget_->layout()->addWidget(windowContainer_);
    }

    rootWidget_->show();
    windowContainer_->setFocus(Qt::OtherFocusReason);
    window_->requestActivate();
    window_->create();

    *surface = QVulkanInstance::surfaceForWindow(window_.get());
    if (*surface == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to create Qt Vulkan window surface");
    }
}

std::vector<const char*> QtWindowBackend::getRequiredInstanceExtensions() const {
    return platformSurfaceExtensions();
}

QWindow& QtWindowBackend::qWindow() {
    return *window_;
}

QWidget& QtWindowBackend::rootWidget() {
    return *rootWidget_;
}

QWidget& QtWindowBackend::controlsWidget() {
    return *controlsWidget_;
}

QWidget& QtWindowBackend::renderWidget() {
    return *windowContainer_;
}

bool QtWindowBackend::eventFilter(QObject* watched, QEvent* event) {
    if (watched != window_.get() && watched != rootWidget_.get()) {
        return QObject::eventFilter(watched, event);
    }

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

    return QObject::eventFilter(watched, event);
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
