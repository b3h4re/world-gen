#include "glfw_window_backend.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

namespace lve {

GlfwWindowBackend::GlfwWindowBackend(int w, int h, std::string name)
    : width_{w}, height_{h}, windowName_{std::move(name)} {
    initWindow();
}

GlfwWindowBackend::~GlfwWindowBackend() {
    glfwDestroyWindow(window_);
    glfwTerminate();
}

bool GlfwWindowBackend::shouldClose() const {
    return glfwWindowShouldClose(window_);
}

void GlfwWindowBackend::requestClose() {
    glfwSetWindowShouldClose(window_, GLFW_TRUE);
}

VkExtent2D GlfwWindowBackend::getExtent() const {
    return {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_)};
}

bool GlfwWindowBackend::wasWindowResized() const {
    return frameBufferResized_;
}

void GlfwWindowBackend::resetWindowResizedFlag() {
    frameBufferResized_ = false;
}

void GlfwWindowBackend::waitEvents() {
    glfwWaitEvents();
}

void GlfwWindowBackend::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) {
    if (glfwCreateWindowSurface(instance, window_, nullptr, surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface");
    }
}

std::vector<const char*> GlfwWindowBackend::getRequiredInstanceExtensions() const {
    uint32_t extensionCount = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    if (extensions == nullptr) {
        throw std::runtime_error("failed to get required GLFW Vulkan instance extensions");
    }

    return {extensions, extensions + extensionCount};
}

void GlfwWindowBackend::frameBufferResizedCallback(GLFWwindow* window, int width, int height) {
    auto backend = reinterpret_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(window));

    backend->frameBufferResized_ = true;
    backend->width_ = width;
    backend->height_ = height;
}

void GlfwWindowBackend::initWindow() {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("failed to initialize GLFW");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(width_, height_, windowName_.c_str(), nullptr, nullptr);
    if (window_ == nullptr) {
        glfwTerminate();
        throw std::runtime_error("failed to create GLFW window");
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, frameBufferResizedCallback);
}

} // namespace lve
