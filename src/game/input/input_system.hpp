#pragma once

#include "game/input/input_state.hpp"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace lve {

struct AppKeyMappings {
    int close = GLFW_KEY_ESCAPE;
    int toggleView = GLFW_KEY_V;
};

class KeyboardControlSystem {
public:
    AppKeyMappings keyMapping{};

    void updateInputState(GLFWwindow *window, AppInputState &input);

private:
    bool closeWasPressed_{false};
    bool toggleViewWasPressed_{false};
};

class MouseControlSystem {
public:
    void updateInputState(GLFWwindow *window, VkExtent2D extent, AppInputState &input);

private:
    bool primaryButtonWasPressed_{false};
};

class AppInputSystem {
public:
    void updateInputState(GLFWwindow *window, VkExtent2D extent, AppInputState &input);

private:
    KeyboardControlSystem keyboardControl_{};
    MouseControlSystem mouseControl_{};
};

} // namespace lve
