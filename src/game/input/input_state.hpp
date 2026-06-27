#pragma once

namespace lve {

struct AppInputState {
    bool escapeJustPressed{false};
    bool viewToggleJustPressed{false};
    bool primaryMouseJustPressed{false};
    float normalizedMouseX{};
    float normalizedMouseY{};
};

} // namespace lve
