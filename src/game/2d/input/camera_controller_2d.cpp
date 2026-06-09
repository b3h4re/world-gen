#include "camera_controller_2d.hpp"

#include <GLFW/glfw3.h>

namespace lve {

void CameraController2d::update(GLFWwindow *window, float frameTime, Camera2d &camera) const {
    glm::vec2 direction{};
    direction.x += glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ? 1.0F : 0.0F;
    direction.x -= glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ? 1.0F : 0.0F;
    direction.y += glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ? 1.0F : 0.0F;
    direction.y -= glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ? 1.0F : 0.0F;

    if (glm::dot(direction, direction) > 0.0F) {
        camera.move(glm::normalize(direction) * frameTime);
    }

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        camera.zoom(1.0F + frameTime);
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        camera.zoom(1.0F / (1.0F + frameTime));
    }
}

} // namespace lve
