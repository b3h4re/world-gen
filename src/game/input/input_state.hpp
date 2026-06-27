#pragma once

namespace lve {

struct AppInputState {
    bool escapeJustPressed{false};
    bool viewToggleJustPressed{false};
    bool primaryMouseJustPressed{false};
    bool cameraMoveLeft{false};
    bool cameraMoveRight{false};
    bool cameraMoveUp{false};
    bool cameraMoveDown{false};
    bool cameraZoomIn{false};
    bool cameraZoomOut{false};
    float normalizedMouseX{};
    float normalizedMouseY{};
};

} // namespace lve
