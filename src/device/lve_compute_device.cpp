#include "lve_compute_device.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*
) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << "\n";
    return VK_FALSE;
}

VkResult createDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger
) {
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
    );
    if (func == nullptr) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
}

void destroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator
) {
    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
    );
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

} // namespace

namespace lve {

LveComputeDevice::LveComputeDevice() {
    createInstance();
    setupDebugMessenger();
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
}

LveComputeDevice::~LveComputeDevice() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);

        if (commandPool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, commandPool_, nullptr);
        }

        vkDestroyDevice(device_, nullptr);
    }

    if (enableValidationLayers && debugMessenger_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        destroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
    }
}

void LveComputeDevice::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "World Generator Compute";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "World Generator";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    const auto extensions = getRequiredExtensions();

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers_.size());
        createInfo.ppEnabledLayerNames = validationLayers_.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute Vulkan instance!");
    }

    verifyRequiredInstanceExtensions();
}

void LveComputeDevice::setupDebugMessenger() {
    if (!enableValidationLayers) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);
    if (createDebugUtilsMessengerEXT(instance_, &createInfo, nullptr, &debugMessenger_) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up compute debug messenger!");
    }
}

void LveComputeDevice::pickPhysicalDevice() {
    std::uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice_ = device;
            break;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a Vulkan device with compute queue support!");
    }

    vkGetPhysicalDeviceFeatures(physicalDevice_, &supportedFeatures_);
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
    std::cout << "compute physical device: " << properties.deviceName << "\n";
}

void LveComputeDevice::createLogicalDevice() {
    const ComputeQueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    if (!indices.isComplete()) {
        throw std::runtime_error("failed to find a complete compute queue family!");
    }

    const float queuePriority = 1.0F;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.computeFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.shaderInt64 = supportedFeatures_.shaderInt64;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.pEnabledFeatures = &deviceFeatures;

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers_.size());
        createInfo.ppEnabledLayerNames = validationLayers_.data();
    }

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute logical device!");
    }

    vkGetDeviceQueue(device_, indices.computeFamily, 0, &computeQueue_);
}

void LveComputeDevice::createCommandPool() {
    const ComputeQueueFamilyIndices queueFamilyIndices = findPhysicalQueueFamilies();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.computeFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute command pool!");
    }
}

bool LveComputeDevice::isDeviceSuitable(VkPhysicalDevice device) const {
    return findQueueFamilies(device).isComplete();
}

std::vector<const char*> LveComputeDevice::getRequiredExtensions() const {
    std::vector<const char*> extensions;

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

bool LveComputeDevice::checkValidationLayerSupport() const {
    std::uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers_) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

ComputeQueueFamilyIndices LveComputeDevice::findQueueFamilies(VkPhysicalDevice device) const {
    ComputeQueueFamilyIndices indices;

    std::uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    bool hasFallbackComputeFamily = false;
    std::uint32_t fallbackComputeFamily = 0;

    for (std::uint32_t i = 0; i < queueFamilies.size(); ++i) {
        const auto& queueFamily = queueFamilies[i];
        if (queueFamily.queueCount == 0 || (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) {
            continue;
        }

        if (!hasFallbackComputeFamily) {
            fallbackComputeFamily = i;
            hasFallbackComputeFamily = true;
        }

        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
            indices.computeFamily = i;
            indices.computeFamilyHasValue = true;
            return indices;
        }
    }

    if (hasFallbackComputeFamily) {
        indices.computeFamily = fallbackComputeFamily;
        indices.computeFamilyHasValue = true;
    }

    return indices;
}

void LveComputeDevice::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
}

void LveComputeDevice::verifyRequiredInstanceExtensions() const {
    std::uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    std::unordered_set<std::string> available;
    for (const auto& extension : extensions) {
        available.insert(extension.extensionName);
    }

    for (const auto* required : getRequiredExtensions()) {
        if (available.find(required) == available.end()) {
            throw std::runtime_error("missing required compute Vulkan instance extension");
        }
    }
}

std::uint32_t LveComputeDevice::findMemoryType(
    std::uint32_t typeFilter,
    VkMemoryPropertyFlags properties
) const {
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);

    for (std::uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1U << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable compute memory type!");
}

void LveComputeDevice::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory
) const {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute buffer!");
    }

    VkMemoryRequirements memRequirements{};
    vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        throw std::runtime_error("failed to allocate compute buffer memory!");
    }

    if (vkBindBufferMemory(device_, buffer, bufferMemory, 0) != VK_SUCCESS) {
        vkFreeMemory(device_, bufferMemory, nullptr);
        vkDestroyBuffer(device_, buffer, nullptr);
        bufferMemory = VK_NULL_HANDLE;
        buffer = VK_NULL_HANDLE;
        throw std::runtime_error("failed to bind compute buffer memory!");
    }
}

VkCommandBuffer LveComputeDevice::beginSingleTimeCommands() const {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    if (vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate compute command buffer!");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        throw std::runtime_error("failed to begin compute command buffer!");
    }

    return commandBuffer;
}

void LveComputeDevice::endSingleTimeCommands(VkCommandBuffer commandBuffer) const {
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        throw std::runtime_error("failed to end compute command buffer!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(computeQueue_, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        throw std::runtime_error("failed to submit compute command buffer!");
    }

    vkQueueWaitIdle(computeQueue_);
    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

void LveComputeDevice::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

} // namespace lve
