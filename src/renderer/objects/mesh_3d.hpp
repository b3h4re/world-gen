#pragma once

#include "device/lve_device.hpp"
#include "model/buffer/lve_buffer.hpp"

#include <cstdint>
#include <span>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace lve {

struct Vertex3d {
    glm::vec3 position{};
    float height{};
    glm::vec3 parentPosition{};
    float parentHeight{};

    static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    static std::vector<VkVertexInputAttributeDescription> getMorphAttributeDescriptions();
    static std::vector<VkVertexInputAttributeDescription>
        getLocalClipmapAttributeDescriptions();
};

class Mesh3dIndexSet {
public:
    Mesh3dIndexSet(LveDevice& device, std::span<const std::vector<std::uint32_t>> variants);

    Mesh3dIndexSet(const Mesh3dIndexSet&) = delete;
    Mesh3dIndexSet& operator=(const Mesh3dIndexSet&) = delete;

    void bind(VkCommandBuffer commandBuffer, std::size_t variant) const;
    void draw(VkCommandBuffer commandBuffer, std::size_t variant) const;
    std::size_t variantCount() const { return indexBuffers_.size(); }

private:
    std::vector<std::unique_ptr<LveBuffer>> indexBuffers_{};
    std::vector<std::uint32_t> indexCounts_{};
};

class Mesh3d {
public:
    Mesh3d(LveDevice &device, const std::vector<Vertex3d> &vertices, const std::vector<std::uint32_t> &indices);
    Mesh3d(
        LveDevice& device,
        const std::vector<Vertex3d>& vertices,
        std::shared_ptr<const Mesh3dIndexSet> indexSet);

    Mesh3d(const Mesh3d &) = delete;
    Mesh3d &operator=(const Mesh3d &) = delete;

    void bind(VkCommandBuffer commandBuffer, std::size_t indexVariant = 0) const;
    void draw(VkCommandBuffer commandBuffer, std::size_t indexVariant = 0) const;

private:
    void uploadVertices(LveDevice& device, const std::vector<Vertex3d>& vertices);

    std::unique_ptr<LveBuffer> vertexBuffer_;
    std::shared_ptr<const Mesh3dIndexSet> indexSet_{};
};

} // namespace lve
