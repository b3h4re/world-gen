#pragma once

#include "device/lve_device.hpp"

#include <cstdint>
#include <vector>
#include <functional>


#include <glm/glm.hpp>
#include <vulkan/vulkan.h>


namespace lve {

glm::vec3 terrainColor(float height);

glm::vec3 terrainBlackAndWhite(float height);

enum class ColorFunctions {
    BlackAndWhite,
    TerrainColorStandard
};

struct ColorMapParams {
    float minHeight{-1.0F};
    float maxHeight{1.0F};
    std::uint32_t resolution{512};
    ColorFunctions mapping{ColorFunctions::TerrainColorStandard};
};


class ColorMapper {
public:
    ColorMapper(LveDevice& device);
    ~ColorMapper();
    static std::vector<std::uint8_t> generateTerrainColorMapRGBA8(ColorMapParams params);

    static std::function<glm::vec3(float)> getColorFunction(ColorFunctions f);

    glm::vec4 mapColor(float height) const;
    const std::vector<std::uint8_t>& colorMapPixelsRGBA8() const { return colorMapPixelsRGBA8_; }

    VkDescriptorImageInfo descriptorInfo() const;
    void recreateColorMap(ColorMapParams params);
    void recreateColorMap();
    ColorMapParams getCurrentParams() const { return params_; }

    void uploadNewPixels(ColorFunctions f);

    void destroyColorMap();



private:
    void init();
    void clear();

    ColorMapParams params_{};
    std::vector<std::uint8_t> colorMapPixelsRGBA8_{};

    VkImage image_ = VK_NULL_HANDLE;
    VkImageView imageView_ = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
    LveDevice& device_;
    static VkSamplerCreateInfo samplerInfo();
    static VkImageCreateInfo imageCreateInfo(uint32_t width, uint32_t height, VkFormat format,
            VkImageTiling tiling, VkImageUsageFlags usage);
    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void createColorMap(ColorMapParams params);

    void initSampler(LveDevice& device);

    void createImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags memoryProperties,
        VkImage& image,
        VkDeviceMemory& imageMemory
    );
    void transitionImageLayout(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue graphicsQueue,
        VkImage image,
        VkFormat format,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    );
    void copyBufferToImage(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue graphicsQueue,
        VkBuffer buffer,
        VkImage image,
        uint32_t width,
        uint32_t height
    );
    VkImageView createImageView(
        VkDevice device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspectFlags
    );

    void uploadPixels(std::vector<std::uint8_t>& pixels);


};

}
