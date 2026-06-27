#include "lve_descriptors.hpp"

#include <cassert>
#include <stdexcept>

namespace lve {

LveDescriptorSetLayout::Builder &LveDescriptorSetLayout::Builder::addBinding(
    std::uint32_t binding,
    VkDescriptorType descriptorType,
    VkShaderStageFlags stageFlags,
    std::uint32_t count) {
    assert(bindings.count(binding) == 0 && "binding already in use");

    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = descriptorType;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags = stageFlags;
    bindings[binding] = layoutBinding;

    return *this;
}

std::unique_ptr<LveDescriptorSetLayout> LveDescriptorSetLayout::Builder::build() const {
    return std::make_unique<LveDescriptorSetLayout>(lveDevice, bindings);
}

LveDescriptorSetLayout::LveDescriptorSetLayout(
    LveDevice &lveDevice,
    std::unordered_map<std::uint32_t, VkDescriptorSetLayoutBinding> bindings)
    : lveDevice_{lveDevice}, bindings_{std::move(bindings)} {
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
    setLayoutBindings.reserve(bindings_.size());
    for (const auto &[binding, layoutBinding] : bindings_) {
        setLayoutBindings.push_back(layoutBinding);
    }

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.bindingCount = static_cast<std::uint32_t>(setLayoutBindings.size());
    descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

    if (vkCreateDescriptorSetLayout(
            lveDevice_.device(),
            &descriptorSetLayoutInfo,
            nullptr,
            &descriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }
}

LveDescriptorSetLayout::~LveDescriptorSetLayout() {
    vkDestroyDescriptorSetLayout(lveDevice_.device(), descriptorSetLayout_, nullptr);
}

LveDescriptorPool::Builder &LveDescriptorPool::Builder::addPoolSize(
    VkDescriptorType descriptorType,
    std::uint32_t count) {
    poolSizes.push_back({descriptorType, count});
    return *this;
}

LveDescriptorPool::Builder &LveDescriptorPool::Builder::setPoolFlags(VkDescriptorPoolCreateFlags flags) {
    poolFlags = flags;
    return *this;
}

LveDescriptorPool::Builder &LveDescriptorPool::Builder::setMaxSets(std::uint32_t count) {
    maxSets = count;
    return *this;
}

std::unique_ptr<LveDescriptorPool> LveDescriptorPool::Builder::build() const {
    return std::make_unique<LveDescriptorPool>(lveDevice, maxSets, poolFlags, poolSizes);
}

LveDescriptorPool::LveDescriptorPool(
    LveDevice &lveDevice,
    std::uint32_t maxSets,
    VkDescriptorPoolCreateFlags poolFlags,
    const std::vector<VkDescriptorPoolSize> &poolSizes)
    : lveDevice_{lveDevice} {
    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    descriptorPoolInfo.pPoolSizes = poolSizes.data();
    descriptorPoolInfo.maxSets = maxSets;
    descriptorPoolInfo.flags = poolFlags;

    if (vkCreateDescriptorPool(lveDevice_.device(), &descriptorPoolInfo, nullptr, &descriptorPool_) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool");
    }
}

LveDescriptorPool::~LveDescriptorPool() {
    vkDestroyDescriptorPool(lveDevice_.device(), descriptorPool_, nullptr);
}

bool LveDescriptorPool::allocateDescriptor(
    VkDescriptorSetLayout descriptorSetLayout,
    VkDescriptorSet &descriptor) const {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;

    return vkAllocateDescriptorSets(lveDevice_.device(), &allocInfo, &descriptor) == VK_SUCCESS;
}

void LveDescriptorPool::freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const {
    vkFreeDescriptorSets(
        lveDevice_.device(),
        descriptorPool_,
        static_cast<std::uint32_t>(descriptors.size()),
        descriptors.data());
}

void LveDescriptorPool::resetPool() {
    vkResetDescriptorPool(lveDevice_.device(), descriptorPool_, 0);
}

LveDescriptorWriter::LveDescriptorWriter(LveDescriptorSetLayout &setLayout, LveDescriptorPool &pool)
    : setLayout_{setLayout}, pool_{pool} {}

LveDescriptorWriter &LveDescriptorWriter::writeBuffer(
    std::uint32_t binding,
    VkDescriptorBufferInfo *bufferInfo) {
    assert(setLayout_.bindings_.count(binding) == 1 && "layout does not contain specified binding");

    const auto &bindingDescription = setLayout_.bindings_.at(binding);
    assert(
        bindingDescription.descriptorCount == 1 &&
        "binding single descriptor info, but binding expects multiple");

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = bindingDescription.descriptorType;
    write.dstBinding = binding;
    write.pBufferInfo = bufferInfo;
    write.descriptorCount = 1;

    writes_.push_back(write);
    return *this;
}

LveDescriptorWriter &LveDescriptorWriter::writeImage(
    std::uint32_t binding,
    VkDescriptorImageInfo *imageInfo) {
    assert(setLayout_.bindings_.count(binding) == 1 && "layout does not contain specified binding");

    const auto &bindingDescription = setLayout_.bindings_.at(binding);
    assert(
        bindingDescription.descriptorCount == 1 &&
        "binding single descriptor info, but binding expects multiple");

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = bindingDescription.descriptorType;
    write.dstBinding = binding;
    write.pImageInfo = imageInfo;
    write.descriptorCount = 1;

    writes_.push_back(write);
    return *this;
}

bool LveDescriptorWriter::build(VkDescriptorSet &set) {
    if (!pool_.allocateDescriptor(setLayout_.getDescriptorSetLayout(), set)) {
        return false;
    }

    overwrite(set);
    return true;
}

void LveDescriptorWriter::overwrite(VkDescriptorSet &set) {
    for (auto &write : writes_) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(
        pool_.lveDevice_.device(),
        static_cast<std::uint32_t>(writes_.size()),
        writes_.data(),
        0,
        nullptr);
}

} // namespace lve
