#include "input_system.hpp"

#include <GLFW/glfw3.h>

namespace lve {

void KeyboardControlSystem::updateInputState(GLFWwindow *window, AppInputState &input) {
    const bool closeIsPressed = glfwGetKey(window, keyMapping.close) == GLFW_PRESS;
    const bool toggleViewIsPressed = glfwGetKey(window, keyMapping.toggleView) == GLFW_PRESS;

    input.escapeJustPressed = closeIsPressed && !closeWasPressed_;
    input.viewToggleJustPressed = toggleViewIsPressed && !toggleViewWasPressed_;

    closeWasPressed_ = closeIsPressed;
    toggleViewWasPressed_ = toggleViewIsPressed;
}

void MouseControlSystem::updateInputState(GLFWwindow *window, VkExtent2D extent, AppInputState &input) {
    double cursorX{};
    double cursorY{};
    glfwGetCursorPos(window, &cursorX, &cursorY);

    input.normalizedMouseX = 2.0F * static_cast<float>(cursorX) / static_cast<float>(extent.width) - 1.0F;
    input.normalizedMouseY = 2.0F * static_cast<float>(cursorY) / static_cast<float>(extent.height) - 1.0F;

    const bool primaryButtonIsPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    input.primaryMouseJustPressed = primaryButtonIsPressed && !primaryButtonWasPressed_;

    primaryButtonWasPressed_ = primaryButtonIsPressed;
}

void AppInputSystem::updateInputState(GLFWwindow *window, VkExtent2D extent, AppInputState &input) {
    input = {};
    keyboardControl_.updateInputState(window, input);
    mouseControl_.updateInputState(window, extent, input);
}

} // namespace lve
