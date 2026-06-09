#include "lve_window.hpp"
#include <vulkan/vulkan_core.h>

#include <stdexcept>

namespace lve {

    LveWindow::LveWindow(int w, int h, std::string name) : width(w), height(h), windowName(name) {
        initWindow();
    }

    LveWindow::~LveWindow() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    bool LveWindow::shouldClose() {
        return glfwWindowShouldClose(window);
    }

    VkExtent2D LveWindow::getExtent() {
        return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }

    bool LveWindow::wasWindowResized() {
        return frameBufferResized;
    }

    void LveWindow::resetWindowResizedFlag() {
        frameBufferResized = false;
    }

    void LveWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
        if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface");
        }
    }

    void LveWindow::frameBufferResizedCallback(GLFWwindow *window, int width, int height) {
        auto lveWindow = reinterpret_cast<LveWindow *>(glfwGetWindowUserPointer(window));

        lveWindow->frameBufferResized = true;
        lveWindow->width = width;
        lveWindow->height = height;
    }

    void LveWindow::initWindow() {
        if (glfwInit() != GLFW_TRUE) {
            throw std::runtime_error("failed to initialize GLFW");
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
        if (window == nullptr) {
            glfwTerminate();
            throw std::runtime_error("failed to create GLFW window");
        }

        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, frameBufferResizedCallback);
    }

}
