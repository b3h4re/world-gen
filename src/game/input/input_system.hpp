#pragma once

#include "game/camera/camera_2d.hpp"
#include "game/input/camera_controller_2d.hpp"
#include "game/camera/camera_3d.hpp"
#include "game/input/camera_controller_3d.hpp"
#include "game/input/input_state.hpp"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <vector>

struct GLFWwindow;

namespace lve {

struct AppKeyMappings {
    int close = GLFW_KEY_ESCAPE;
    int toggleView = GLFW_KEY_V;
    int cameraMoveLeft = GLFW_KEY_A;
    int cameraMoveRight = GLFW_KEY_D;
    int cameraMoveUp = GLFW_KEY_W;
    int cameraMoveDown = GLFW_KEY_S;
    int cameraZoomIn = GLFW_KEY_E;
    int cameraZoomOut = GLFW_KEY_Q;
};

struct CameraUpdateTarget {
    enum class Type {
        Camera2d,
        Camera3d
    };

    Type type;
    bool active{false};
    Camera2d *camera2d{nullptr};
    Camera3d *camera3d{nullptr};
};

CameraUpdateTarget makeCameraTarget(Camera2d &camera, bool active);
CameraUpdateTarget makeCameraTarget(Camera3d &camera, bool active);

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
    void updateCameras(
        const AppInputState &input,
        float frameTime,
        float aspectRatio,
        std::vector<CameraUpdateTarget> &targets);

private:
    KeyboardControlSystem keyboardControl_{};
    MouseControlSystem mouseControl_{};
    CameraController2d cameraController2d_{};
    CameraController3d cameraController3d_{};
};

} // namespace lve
