#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace lve {

class WindowSurface {
public:
    virtual ~WindowSurface() = default;

    virtual bool shouldClose() const = 0;
    virtual void requestClose() = 0;
    virtual VkExtent2D getExtent() const = 0;
    virtual bool wasWindowResized() const = 0;
    virtual void resetWindowResizedFlag() = 0;
    virtual void waitEvents() = 0;
    virtual void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) = 0;
    virtual std::vector<const char*> getRequiredInstanceExtensions() const = 0;
};

} // namespace lve
