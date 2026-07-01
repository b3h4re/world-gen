#include "qt_window_backend.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QString>
#include <QtGui/QWindow>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtGui/QPlatformSurfaceEvent>


#include <array>
#include <algorithm>
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

    auto* layout = new QHBoxLayout(rootWidget_.get());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    controlsWidget_ = new QWidget{rootWidget_.get()};
    controlsWidget_->setObjectName("terrainControls");
    controlsWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(controlsWidget_);
}

QtWindowBackend::~QtWindowBackend() {
    if (windowContainer_) {
        windowContainer_->removeEventFilter(this);
    }
    if (renderParentWidget_) {
        renderParentWidget_->removeEventFilter(this);
    }
    if (rootWidget_) {
        rootWidget_->removeEventFilter(this);
    }
    if (window_) {
        window_->setParent(nullptr);
        window_->removeEventFilter(this);
        window_->destroy();
    }
}

bool QtWindowBackend::shouldClose() const {
    return closeRequested_;
}

void QtWindowBackend::requestClose() {
    closeRequested_ = true;
}

VkExtent2D QtWindowBackend::getExtent() const {
    if (windowContainer_ != nullptr) {
        return {
            static_cast<uint32_t>(std::max(windowContainer_->width(), 1)),
            static_cast<uint32_t>(std::max(windowContainer_->height(), 1)),
        };
    }

    if (renderParentWidget_ != nullptr) {
        return {
            static_cast<uint32_t>(std::max(renderParentWidget_->width(), 1)),
            static_cast<uint32_t>(std::max(renderParentWidget_->height(), 1)),
        };
    }

    return {
        static_cast<uint32_t>(std::max(window_->width(), 1)),
        static_cast<uint32_t>(std::max(window_->height(), 1)),
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
        QWidget* parentWidget = renderParentWidget_ != nullptr ? renderParentWidget_ : rootWidget_.get();
        windowContainer_ = QWidget::createWindowContainer(window_.get(), parentWidget);
        windowContainer_->setFocusPolicy(Qt::StrongFocus);
        windowContainer_->setMouseTracking(true);
        windowContainer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        windowContainer_->installEventFilter(this);
        setRenderParent(*parentWidget);
    }

    rootWidget_->show();
    windowContainer_->setFocus(Qt::OtherFocusReason);
    window_->requestActivate();
    window_->requestUpdate();
    window_->create();

    *surface = QVulkanInstance::surfaceForWindow(window_.get());
    if (*surface == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to create Qt Vulkan window surface");
    }
}

void QtWindowBackend::destroyWindowSurface(VkInstance, VkSurfaceKHR) {
    if (window_) {
        window_->destroy();
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

QWidget& QtWindowBackend::renderParentWidget() {
    return renderParentWidget_ != nullptr ? *renderParentWidget_ : *windowContainer_;
}

void QtWindowBackend::setRenderParent(QWidget& renderParent) {
    if (renderParentWidget_ != nullptr && renderParentWidget_ != &renderParent) {
        renderParentWidget_->removeEventFilter(this);
    }

    if (windowContainer_ != nullptr && windowContainer_->parentWidget() != &renderParent) {
        if (auto* oldParent = windowContainer_->parentWidget()) {
            if (auto* oldLayout = oldParent->layout()) {
                oldLayout->removeWidget(windowContainer_);
            }
        }
        windowContainer_->setParent(&renderParent);
    }

    renderParentWidget_ = &renderParent;
    renderParentWidget_->installEventFilter(this);
    renderParentWidget_->setFocusPolicy(Qt::StrongFocus);
    renderParentWidget_->setMouseTracking(true);
    renderParentWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* layout = renderParentWidget_->layout();
    if (layout == nullptr) {
        layout = new QVBoxLayout(renderParentWidget_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }

    if (windowContainer_ != nullptr) {
        windowContainer_->setFocusPolicy(Qt::StrongFocus);
        windowContainer_->setMouseTracking(true);
        layout->addWidget(windowContainer_);
        windowContainer_->show();
        windowContainer_->setFocus(Qt::OtherFocusReason);
        window_->requestActivate();
    }

    frameBufferResized_ = true;
}
void QtWindowBackend::detachRenderParent() {
    if (renderParentWidget_ != nullptr) {
        renderParentWidget_->removeEventFilter(this);
    }

    if (windowContainer_ != nullptr) {
        if (auto* oldParent = windowContainer_->parentWidget()) {
            if (auto* oldLayout = oldParent->layout()) {
                oldLayout->removeWidget(windowContainer_);
            }
        }

        windowContainer_->hide();

        if (rootWidget_ != nullptr && windowContainer_->parentWidget() != rootWidget_.get()) {
            windowContainer_->setParent(rootWidget_.get());
        }
    }

    renderParentWidget_ = nullptr;
}

void QtWindowBackend::setSurfaceAboutToBeDestroyedCallback(std::function<void()> callback) {
    surfaceAboutToBeDestroyedCallback_ = std::move(callback);
}

bool QtWindowBackend::eventFilter(QObject* watched, QEvent* event) {
    if (watched != window_.get()
        && watched != rootWidget_.get()
        && watched != renderParentWidget_
        && watched != windowContainer_) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
        case QEvent::Close:
            closeRequested_ = true;
            event->ignore();
            return true;
        case QEvent::Resize:
            frameBufferResized_ = true;
            break;
        case QEvent::PlatformSurface: {
            QPlatformSurfaceEvent* surfaceEvent = static_cast<QPlatformSurfaceEvent*>(event);

            if (surfaceEvent->surfaceEventType()
                == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
                closeRequested_ = true;
                if (surfaceAboutToBeDestroyedCallback_) {
                    surfaceAboutToBeDestroyedCallback_();
                }

            }

            if (surfaceEvent->surfaceEventType()
                == QPlatformSurfaceEvent::SurfaceCreated) {
                frameBufferResized_ = true;
            }

            break;
        }
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
