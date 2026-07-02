#pragma once

#include "device/lve_device.hpp"
#include "model/buffer/lve_buffer.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace lve {

struct Vertex3d {
    glm::vec3 position;
    float height;

    static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

class Mesh3d {
public:
    Mesh3d(LveDevice &device, const std::vector<Vertex3d> &vertices, const std::vector<std::uint32_t> &indices);

    Mesh3d(const Mesh3d &) = delete;
    Mesh3d &operator=(const Mesh3d &) = delete;

    void bind(VkCommandBuffer commandBuffer) const;
    void draw(VkCommandBuffer commandBuffer) const;

private:
    std::unique_ptr<LveBuffer> vertexBuffer_;
    std::unique_ptr<LveBuffer> indexBuffer_;
    std::uint32_t indexCount_{};
};

} // namespace lve
