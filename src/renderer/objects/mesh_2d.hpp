#pragma once

#include "device/lve_device.hpp"
#include "model/buffer/lve_buffer.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace lve {

struct Vertex2d {
    glm::vec2 position;
    glm::vec3 color;

    static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

class Mesh2d {
public:
    Mesh2d(LveDevice &device, const std::vector<Vertex2d> &vertices, const std::vector<std::uint32_t> &indices);

    Mesh2d(const Mesh2d &) = delete;
    Mesh2d &operator=(const Mesh2d &) = delete;

    void bind(VkCommandBuffer commandBuffer) const;
    void draw(VkCommandBuffer commandBuffer) const;

private:
    std::unique_ptr<LveBuffer> vertexBuffer_;
    std::unique_ptr<LveBuffer> indexBuffer_;
    std::uint32_t indexCount_{};
};

} // namespace lve
