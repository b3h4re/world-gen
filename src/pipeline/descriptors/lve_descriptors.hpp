#pragma once

#include "device/lve_device.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace lve {

class LveDescriptorSetLayout {
public:
    class Builder {
    public:
        explicit Builder(LveDevice &lveDevice) : lveDevice{lveDevice} {}

        Builder &addBinding(
            std::uint32_t binding,
            VkDescriptorType descriptorType,
            VkShaderStageFlags stageFlags,
            std::uint32_t count = 1);
        std::unique_ptr<LveDescriptorSetLayout> build() const;

    private:
        LveDevice &lveDevice;
        std::unordered_map<std::uint32_t, VkDescriptorSetLayoutBinding> bindings{};
    };

    LveDescriptorSetLayout(
        LveDevice &lveDevice,
        std::unordered_map<std::uint32_t, VkDescriptorSetLayoutBinding> bindings);
    ~LveDescriptorSetLayout();

    LveDescriptorSetLayout(const LveDescriptorSetLayout &) = delete;
    LveDescriptorSetLayout &operator=(const LveDescriptorSetLayout &) = delete;

    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout_; }

private:
    LveDevice &lveDevice_;
    VkDescriptorSetLayout descriptorSetLayout_{VK_NULL_HANDLE};
    std::unordered_map<std::uint32_t, VkDescriptorSetLayoutBinding> bindings_;

    friend class LveDescriptorWriter;
};

class LveDescriptorPool {
public:
    class Builder {
    public:
        explicit Builder(LveDevice &lveDevice) : lveDevice{lveDevice} {}

        Builder &addPoolSize(VkDescriptorType descriptorType, std::uint32_t count);
        Builder &setPoolFlags(VkDescriptorPoolCreateFlags flags);
        Builder &setMaxSets(std::uint32_t count);
        std::unique_ptr<LveDescriptorPool> build() const;

    private:
        LveDevice &lveDevice;
        std::vector<VkDescriptorPoolSize> poolSizes{};
        std::uint32_t maxSets{1000};
        VkDescriptorPoolCreateFlags poolFlags{0};
    };

    LveDescriptorPool(
        LveDevice &lveDevice,
        std::uint32_t maxSets,
        VkDescriptorPoolCreateFlags poolFlags,
        const std::vector<VkDescriptorPoolSize> &poolSizes);
    ~LveDescriptorPool();

    LveDescriptorPool(const LveDescriptorPool &) = delete;
    LveDescriptorPool &operator=(const LveDescriptorPool &) = delete;

    bool allocateDescriptor(VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const;
    void freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const;
    void resetPool();

private:
    LveDevice &lveDevice_;
    VkDescriptorPool descriptorPool_{VK_NULL_HANDLE};

    friend class LveDescriptorWriter;
};

class LveDescriptorWriter {
public:
    LveDescriptorWriter(LveDescriptorSetLayout &setLayout, LveDescriptorPool &pool);

    LveDescriptorWriter &writeBuffer(std::uint32_t binding, VkDescriptorBufferInfo *bufferInfo);
    LveDescriptorWriter &writeImage(std::uint32_t binding, VkDescriptorImageInfo *imageInfo);

    bool build(VkDescriptorSet &set);
    void overwrite(VkDescriptorSet &set);

private:
    LveDescriptorSetLayout &setLayout_;
    LveDescriptorPool &pool_;
    std::vector<VkWriteDescriptorSet> writes_;
};

} // namespace lve
