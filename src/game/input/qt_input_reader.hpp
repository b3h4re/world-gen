#pragma once

#include "game/input/input_reader.hpp"

#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/Qt>

class QEvent;
class QWindow;

namespace lve {

struct QtKeyMappings {
    int close = Qt::Key_Escape;
    int toggleView = Qt::Key_V;
    int cameraMoveLeft = Qt::Key_A;
    int cameraMoveRight = Qt::Key_D;
    int cameraMoveUp = Qt::Key_W;
    int cameraMoveDown = Qt::Key_S;
    int cameraZoomIn = Qt::Key_E;
    int cameraZoomOut = Qt::Key_Q;
};

class QtInputReader : public QObject, public InputReader {
public:
    explicit QtInputReader(QWindow& window);
    ~QtInputReader() override;

    QtInputReader(const QtInputReader&) = delete;
    QtInputReader& operator=(const QtInputReader&) = delete;

    AppInputState readInputState(VkExtent2D extent) override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void handleKeyPress(int key, bool autoRepeat);
    void handleKeyRelease(int key, bool autoRepeat);
    void setKeyDown(int key, bool down);

    QWindow& window_;
    QtKeyMappings keyMapping_{};
    QPointF mousePosition_{};
    bool closeDown_{false};
    bool toggleViewDown_{false};
    bool cameraMoveLeft_{false};
    bool cameraMoveRight_{false};
    bool cameraMoveUp_{false};
    bool cameraMoveDown_{false};
    bool cameraZoomIn_{false};
    bool cameraZoomOut_{false};
    bool escapeJustPressed_{false};
    bool viewToggleJustPressed_{false};
    bool primaryMouseJustPressed_{false};
};

} // namespace lve
