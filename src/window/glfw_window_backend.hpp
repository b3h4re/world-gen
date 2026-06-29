#pragma once

#include "window/window_surface.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

namespace lve {

class GlfwWindowBackend : public WindowSurface {
public:
    GlfwWindowBackend(int w, int h, std::string name);
    ~GlfwWindowBackend() override;

    GlfwWindowBackend(const GlfwWindowBackend&) = delete;
    GlfwWindowBackend& operator=(const GlfwWindowBackend&) = delete;

    bool shouldClose() const override;
    void requestClose() override;
    VkExtent2D getExtent() const override;
    bool wasWindowResized() const override;
    void resetWindowResizedFlag() override;
    void waitEvents() override;
    GLFWwindow* getGLFWwindow() const { return window_; }

    void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) override;
    std::vector<const char*> getRequiredInstanceExtensions() const override;

private:
    static void frameBufferResizedCallback(GLFWwindow* window, int width, int height);
    void initWindow();

    int width_;
    int height_;
    bool frameBufferResized_{false};

    std::string windowName_;
    GLFWwindow* window_{nullptr};
};

} // namespace lve
