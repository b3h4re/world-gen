#include "mesh_3d.hpp"

#include <cstddef>
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

Mesh3d::Mesh3d(
    LveDevice &device,
    const std::vector<Vertex3d> &vertices,
    const std::vector<std::uint32_t> &indices)
    : indexCount_{static_cast<std::uint32_t>(indices.size())} {
    if (vertices.empty() || indices.empty()) {
        throw std::invalid_argument("3D mesh requires vertices and indices");
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

    LveBuffer indexStaging{
        device,
        sizeof(std::uint32_t),
        indexCount_,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    indexStaging.map();
    indexStaging.writeToBuffer(const_cast<std::uint32_t *>(indices.data()));

    indexBuffer_ = std::make_unique<LveBuffer>(
        device,
        sizeof(std::uint32_t),
        indexCount_,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    device.copyBuffer(indexStaging.getBuffer(), indexBuffer_->getBuffer(), sizeof(std::uint32_t) * indices.size());
}

void Mesh3d::bind(VkCommandBuffer commandBuffer) const {
    const VkBuffer buffers[] = {vertexBuffer_->getBuffer()};
    constexpr VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer_->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

void Mesh3d::draw(VkCommandBuffer commandBuffer) const {
    vkCmdDrawIndexed(commandBuffer, indexCount_, 1, 0, 0, 0);
}

} // namespace lve
