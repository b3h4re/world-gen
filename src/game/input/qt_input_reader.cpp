#include "qt_input_reader.hpp"

#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWindow>

namespace lve {

QtInputReader::QtInputReader(QWindow& window) : window_{window} {
    window_.installEventFilter(this);
}

QtInputReader::~QtInputReader() {
    window_.removeEventFilter(this);
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
    if (watched != &window_) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
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
            mousePosition_ = mouseEvent->position();
            break;
        }
        case QEvent::MouseButtonPress: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            mousePosition_ = mouseEvent->position();
            if (mouseEvent->button() == Qt::LeftButton) {
                primaryMouseJustPressed_ = true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            mousePosition_ = mouseEvent->position();
            break;
        }
        default:
            break;
    }

    return QObject::eventFilter(watched, event);
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

} // namespace lve
