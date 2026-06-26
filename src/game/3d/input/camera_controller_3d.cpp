#include "camera_controller_3d.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

namespace lve {

void CameraController3d::update(GLFWwindow *window, float frameTime, Camera3d &camera) {
    constexpr float rotateSpeed = 1.5F;
    constexpr float zoomSpeed = 2.0F;

    yaw_ += glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ? rotateSpeed * frameTime : 0.0F;
    yaw_ -= glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ? rotateSpeed * frameTime : 0.0F;
    pitch_ += glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ? rotateSpeed * frameTime : 0.0F;
    pitch_ -= glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ? rotateSpeed * frameTime : 0.0F;
    distance_ += glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS ? zoomSpeed * frameTime : 0.0F;
    distance_ -= glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS ? zoomSpeed * frameTime : 0.0F;

    pitch_ = std::clamp(pitch_, 0.1F, glm::half_pi<float>() - 0.05F);
    distance_ = std::clamp(distance_, 1.2F, 8.0F);

    const glm::vec3 target{0.0F, 0.0F, 0.0F};
    const glm::vec3 position{
        distance_ * std::cos(pitch_) * std::sin(yaw_),
        distance_ * std::sin(pitch_),
        distance_ * std::cos(pitch_) * std::cos(yaw_)
    };
    camera.setViewTarget(position, target);
}

} // namespace lve
