#include "qt_input_reader.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWindow>
#include <QtWidgets/QWidget>

namespace lve {

QtInputReader::QtInputReader(QWindow& window) : window_{window} {
    installOn(*QCoreApplication::instance());
    installOn(window_);
}

QtInputReader::QtInputReader(QWindow& window, QWidget& rootWidget, QWidget& renderWidget)
    : QtInputReader{window, rootWidget, renderWidget, renderWidget} {}

QtInputReader::QtInputReader(QWindow& window, QWidget& rootWidget, QWidget& renderParentWidget, QWidget& renderWidget)
    : window_{window}, rootWidget_{&rootWidget}, renderParentWidget_{&renderParentWidget}, renderWidget_{&renderWidget} {
    installOn(*QCoreApplication::instance());
    installOn(window_);
    installOn(*rootWidget_);
    installOn(*renderParentWidget_);
    installOn(*renderWidget_);
    rootWidget_->setFocusPolicy(Qt::StrongFocus);
    renderParentWidget_->setFocusPolicy(Qt::StrongFocus);
    renderParentWidget_->setMouseTracking(true);
    renderWidget_->setFocusPolicy(Qt::StrongFocus);
    renderWidget_->setMouseTracking(true);
    focusRenderTarget();
}

QtInputReader::~QtInputReader() {
    if (QCoreApplication::instance() != nullptr) {
        removeFrom(*QCoreApplication::instance());
    }
    removeFrom(window_);
    if (rootWidget_ != nullptr) {
        removeFrom(*rootWidget_);
    }
    if (renderParentWidget_ != nullptr && renderParentWidget_ != renderWidget_) {
        removeFrom(*renderParentWidget_);
    }
    if (renderWidget_ != nullptr) {
        removeFrom(*renderWidget_);
    }
}

AppInputState QtInputReader::readInputState(VkExtent2D extent) {
    AppInputState input{};
    input.escapeJustPressed = escapeJustPressed_;
    input.viewToggleJustPressed = viewToggleJustPressed_;
    input.primaryMouseJustPressed = primaryMouseJustPressed_;
    input.cameraMoveLeft = cameraMoveLeft_;
    input.cameraMoveRight = cameraMoveRight_;
    input.cameraMoveUp = cameraMoveUp_;
    input.cameraMoveDown = cameraMoveDown_;
    input.cameraZoomIn = cameraZoomIn_;
    input.cameraZoomOut = cameraZoomOut_;

    if (extent.width > 0 && extent.height > 0) {
        input.normalizedMouseX = 2.0F * static_cast<float>(mousePosition_.x()) / static_cast<float>(extent.width) - 1.0F;
        input.normalizedMouseY = 2.0F * static_cast<float>(mousePosition_.y()) / static_cast<float>(extent.height) - 1.0F;
    }

    escapeJustPressed_ = false;
    viewToggleJustPressed_ = false;
    primaryMouseJustPressed_ = false;

    return input;
}

bool QtInputReader::eventFilter(QObject* watched, QEvent* event) {
    switch (event->type()) {
        case QEvent::FocusIn:
            if (watched == renderParentWidget_) {
                focusRenderTarget();
            }
            break;
        case QEvent::FocusOut:
            if (watched == renderParentWidget_) {
                releaseRenderKeyboard();
            }
            break;
        case QEvent::KeyPress: {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            handleKeyPress(keyEvent->key(), keyEvent->isAutoRepeat());
            break;
        }
        case QEvent::KeyRelease: {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            handleKeyRelease(keyEvent->key(), keyEvent->isAutoRepeat());
            break;
        }
        case QEvent::MouseMove: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            mousePosition_ = window_.mapFromGlobal(mouseEvent->globalPosition().toPoint());
            break;
        }
        case QEvent::MouseButtonPress: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (isRenderObject(watched)) {
                focusRenderTarget();
            }
            mousePosition_ = window_.mapFromGlobal(mouseEvent->globalPosition().toPoint());
            if (mouseEvent->button() == Qt::LeftButton) {
                primaryMouseJustPressed_ = true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            mousePosition_ = window_.mapFromGlobal(mouseEvent->globalPosition().toPoint());
            break;
        }
        default:
            break;
    }

    return QObject::eventFilter(watched, event);
}

void QtInputReader::installOn(QObject& object) {
    object.installEventFilter(this);
}

void QtInputReader::removeFrom(QObject& object) {
    object.removeEventFilter(this);
}

void QtInputReader::handleKeyPress(int key, bool autoRepeat) {
    if (!autoRepeat) {
        if (key == keyMapping_.close && !closeDown_) {
            escapeJustPressed_ = true;
        }
        if (key == keyMapping_.toggleView && !toggleViewDown_) {
            viewToggleJustPressed_ = true;
        }
    }

    setKeyDown(key, true);
}

void QtInputReader::handleKeyRelease(int key, bool autoRepeat) {
    if (autoRepeat) {
        return;
    }

    setKeyDown(key, false);
}

void QtInputReader::setKeyDown(int key, bool down) {
    if (key == keyMapping_.close) {
        closeDown_ = down;
    } else if (key == keyMapping_.toggleView) {
        toggleViewDown_ = down;
    } else if (key == keyMapping_.cameraMoveLeft) {
        cameraMoveLeft_ = down;
    } else if (key == keyMapping_.cameraMoveRight) {
        cameraMoveRight_ = down;
    } else if (key == keyMapping_.cameraMoveUp) {
        cameraMoveUp_ = down;
    } else if (key == keyMapping_.cameraMoveDown) {
        cameraMoveDown_ = down;
    } else if (key == keyMapping_.cameraZoomIn) {
        cameraZoomIn_ = down;
    } else if (key == keyMapping_.cameraZoomOut) {
        cameraZoomOut_ = down;
    }
}

bool QtInputReader::isRenderObject(QObject* object) const {
    return object == &window_ || object == renderParentWidget_ || object == renderWidget_;
}

void QtInputReader::focusRenderTarget() {
    if (renderParentWidget_ != nullptr && renderParentWidget_->isVisible()) {
        if (!renderParentWidget_->hasFocus()) {
            renderParentWidget_->setFocus(Qt::OtherFocusReason);
        }
        renderParentWidget_->grabKeyboard();
    }
    window_.requestActivate();
}

void QtInputReader::releaseRenderKeyboard() {
    if (renderParentWidget_ != nullptr && renderParentWidget_->keyboardGrabber() == renderParentWidget_) {
        renderParentWidget_->releaseKeyboard();
    }
    closeDown_ = false;
    toggleViewDown_ = false;
    cameraMoveLeft_ = false;
    cameraMoveRight_ = false;
    cameraMoveUp_ = false;
    cameraMoveDown_ = false;
    cameraZoomIn_ = false;
    cameraZoomOut_ = false;
}

} // namespace lve
