#include "mesh_3d.hpp"

#include <cstddef>
#include <limits>
#include <stdexcept>

namespace lve {

std::vector<VkVertexInputBindingDescription> Vertex3d::getBindingDescriptions() {
    return {{0, sizeof(Vertex3d), VK_VERTEX_INPUT_RATE_VERTEX}};
}

std::vector<VkVertexInputAttributeDescription> Vertex3d::getAttributeDescriptions() {
    return {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3d, position)},
        {1, 0, VK_FORMAT_R32_SFLOAT, offsetof(Vertex3d, height)},
    };
}

std::vector<VkVertexInputAttributeDescription> Vertex3d::getMorphAttributeDescriptions() {
    return {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3d, position)},
        {1, 0, VK_FORMAT_R32_SFLOAT, offsetof(Vertex3d, height)},
        {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3d, parentPosition)},
        {3, 0, VK_FORMAT_R32_SFLOAT, offsetof(Vertex3d, parentHeight)},
    };
}

Mesh3dIndexSet::Mesh3dIndexSet(
        LveDevice& device,
        std::span<const std::vector<std::uint32_t>> variants) {
    if (variants.empty()) {
        throw std::invalid_argument("3D mesh index set requires at least one variant");
    }
    indexBuffers_.reserve(variants.size());
    indexCounts_.reserve(variants.size());
    for (const std::vector<std::uint32_t>& indices : variants) {
        if (indices.empty() || indices.size() > std::numeric_limits<std::uint32_t>::max()) {
            throw std::invalid_argument("3D mesh index variant is empty or too large");
        }
        const auto indexCount = static_cast<std::uint32_t>(indices.size());
        LveBuffer staging{
            device,
            sizeof(std::uint32_t),
            indexCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
        staging.map();
        staging.writeToBuffer(const_cast<std::uint32_t*>(indices.data()));

        auto buffer = std::make_unique<LveBuffer>(
            device,
            sizeof(std::uint32_t),
            indexCount,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        device.copyBuffer(
            staging.getBuffer(),
            buffer->getBuffer(),
            sizeof(std::uint32_t) * indices.size());
        indexBuffers_.push_back(std::move(buffer));
        indexCounts_.push_back(indexCount);
    }
}

void Mesh3dIndexSet::bind(VkCommandBuffer commandBuffer, std::size_t variant) const {
    if (variant >= indexBuffers_.size()) {
        throw std::out_of_range("3D mesh index variant is out of range");
    }
    vkCmdBindIndexBuffer(
        commandBuffer,
        indexBuffers_[variant]->getBuffer(),
        0,
        VK_INDEX_TYPE_UINT32);
}

void Mesh3dIndexSet::draw(VkCommandBuffer commandBuffer, std::size_t variant) const {
    if (variant >= indexCounts_.size()) {
        throw std::out_of_range("3D mesh index variant is out of range");
    }
    vkCmdDrawIndexed(commandBuffer, indexCounts_[variant], 1, 0, 0, 0);
}

Mesh3d::Mesh3d(
        LveDevice& device,
        const std::vector<Vertex3d>& vertices,
        const std::vector<std::uint32_t>& indices)
    : indexSet_{std::make_shared<Mesh3dIndexSet>(
          device,
          std::span<const std::vector<std::uint32_t>>{&indices, 1})} {
    uploadVertices(device, vertices);
}

Mesh3d::Mesh3d(
        LveDevice& device,
        const std::vector<Vertex3d>& vertices,
        std::shared_ptr<const Mesh3dIndexSet> indexSet)
    : indexSet_{std::move(indexSet)} {
    if (indexSet_ == nullptr) {
        throw std::invalid_argument("3D mesh requires an index set");
    }
    uploadVertices(device, vertices);
}

void Mesh3d::uploadVertices(LveDevice& device, const std::vector<Vertex3d>& vertices) {
    if (vertices.empty() || vertices.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("3D mesh requires a supported non-empty vertex set");
    }

    LveBuffer vertexStaging{
        device,
        sizeof(Vertex3d),
        static_cast<std::uint32_t>(vertices.size()),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    vertexStaging.map();
    vertexStaging.writeToBuffer(const_cast<Vertex3d *>(vertices.data()));

    vertexBuffer_ = std::make_unique<LveBuffer>(
        device,
        sizeof(Vertex3d),
        static_cast<std::uint32_t>(vertices.size()),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    device.copyBuffer(vertexStaging.getBuffer(), vertexBuffer_->getBuffer(), sizeof(Vertex3d) * vertices.size());
}

void Mesh3d::bind(VkCommandBuffer commandBuffer, std::size_t indexVariant) const {
    const VkBuffer buffers[] = {vertexBuffer_->getBuffer()};
    constexpr VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
    indexSet_->bind(commandBuffer, indexVariant);
}

void Mesh3d::draw(VkCommandBuffer commandBuffer, std::size_t indexVariant) const {
    indexSet_->draw(commandBuffer, indexVariant);
}

} // namespace lve
