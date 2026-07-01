#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace lve {

struct ComputeQueueFamilyIndices {
    std::uint32_t computeFamily{};
    bool computeFamilyHasValue{false};

    bool isComplete() const { return computeFamilyHasValue; }
};

class LveComputeDevice {
public:
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    LveComputeDevice();
    ~LveComputeDevice();

    LveComputeDevice(const LveComputeDevice&) = delete;
    LveComputeDevice& operator=(const LveComputeDevice&) = delete;
    LveComputeDevice(LveComputeDevice&&) = delete;
    LveComputeDevice& operator=(LveComputeDevice&&) = delete;

    VkCommandPool getCommandPool() const { return commandPool_; }
    VkDevice device() const { return device_; }
    VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
    VkQueue computeQueue() const { return computeQueue_; }
    bool supportsShaderInt64() const { return supportedFeatures_.shaderInt64 == VK_TRUE; }

    std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    ComputeQueueFamilyIndices findPhysicalQueueFamilies() const { return findQueueFamilies(physicalDevice_); }

    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory
    ) const;
    VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) const;
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const;

    VkPhysicalDeviceProperties properties{};

private:
    void createInstance();
    void setupDebugMessenger();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();

    bool isDeviceSuitable(VkPhysicalDevice device) const;
    std::vector<const char*> getRequiredExtensions() const;
    bool checkValidationLayerSupport() const;
    ComputeQueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const;
    void verifyRequiredInstanceExtensions() const;

    VkInstance instance_{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debugMessenger_{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};
    VkPhysicalDeviceFeatures supportedFeatures_{};
    VkCommandPool commandPool_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
    VkQueue computeQueue_{VK_NULL_HANDLE};

    const std::vector<const char*> validationLayers_{"VK_LAYER_KHRONOS_validation"};
};

} // namespace lve
