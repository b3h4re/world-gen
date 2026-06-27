#include "input_system.hpp"

#include <GLFW/glfw3.h>

namespace lve {

void KeyboardControlSystem::updateInputState(GLFWwindow *window, AppInputState &input) {
    const bool closeIsPressed = glfwGetKey(window, keyMapping.close) == GLFW_PRESS;
    const bool toggleViewIsPressed = glfwGetKey(window, keyMapping.toggleView) == GLFW_PRESS;

    input.escapeJustPressed = closeIsPressed && !closeWasPressed_;
    input.viewToggleJustPressed = toggleViewIsPressed && !toggleViewWasPressed_;
    input.cameraMoveLeft = glfwGetKey(window, keyMapping.cameraMoveLeft) == GLFW_PRESS;
    input.cameraMoveRight = glfwGetKey(window, keyMapping.cameraMoveRight) == GLFW_PRESS;
    input.cameraMoveUp = glfwGetKey(window, keyMapping.cameraMoveUp) == GLFW_PRESS;
    input.cameraMoveDown = glfwGetKey(window, keyMapping.cameraMoveDown) == GLFW_PRESS;
    input.cameraZoomIn = glfwGetKey(window, keyMapping.cameraZoomIn) == GLFW_PRESS;
    input.cameraZoomOut = glfwGetKey(window, keyMapping.cameraZoomOut) == GLFW_PRESS;

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

CameraUpdateTarget makeCameraTarget(Camera2d &camera, bool active) {
    return {
        CameraUpdateTarget::Type::Camera2d,
        active,
        &camera,
        nullptr,
    };
}

CameraUpdateTarget makeCameraTarget(Camera3d &camera, bool active) {
    return {
        CameraUpdateTarget::Type::Camera3d,
        active,
        nullptr,
        &camera,
    };
}

void AppInputSystem::updateCameras(
    const AppInputState &input,
    float frameTime,
    float aspectRatio,
    std::vector<CameraUpdateTarget> &targets) {
    for (auto &target : targets) {
        switch (target.type) {
            case CameraUpdateTarget::Type::Camera2d:
                target.camera2d->setAspectRatio(aspectRatio);
                if (target.active) {
                    cameraController2d_.update(input, frameTime, *target.camera2d);
                }
                break;
            case CameraUpdateTarget::Type::Camera3d:
                target.camera3d->setPerspectiveProjection(glm::radians(50.0F), aspectRatio, 0.1F, 20.0F);
                if (target.active) {
                    cameraController3d_.update(input, frameTime, *target.camera3d);
                }
                break;
        }
    }
}

} // namespace lve
