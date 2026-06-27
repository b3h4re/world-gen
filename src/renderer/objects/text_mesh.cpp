#include "text_mesh.hpp"

#include <cstddef>

namespace lve {

std::vector<VkVertexInputBindingDescription> TextVertex::getBindingDescriptions() {
    return {{0, sizeof(TextVertex), VK_VERTEX_INPUT_RATE_VERTEX}};
}

std::vector<VkVertexInputAttributeDescription> TextVertex::getAttributeDescriptions() {
    return {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TextVertex, position)},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TextVertex, uv)},
        {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TextVertex, color)},
    };
}

TextMesh::TextMesh(LveDevice &device, FontAtlas &font, std::string_view text, glm::vec2 position) {
    constexpr glm::vec3 textColor{1.0F, 1.0F, 1.0F};
    std::vector<TextVertex> vertices;
    std::vector<std::uint32_t> indices;

    glm::vec2 pen = position;
    for (const char character : text) {
        if (character == '\n') {
            pen.x = position.x;
            pen.y += font.pixelHeight;
            continue;
        }

        const auto *glyph = font.glyph(static_cast<unsigned char>(character));
        if (glyph == nullptr) {
            continue;
        }

        const glm::vec2 min = pen + glyph->bearing;
        const glm::vec2 max = min + glyph->size;
        const auto base = static_cast<std::uint32_t>(vertices.size());

        vertices.push_back({{min.x, min.y}, {glyph->uvMin.x, glyph->uvMin.y}, textColor});
        vertices.push_back({{max.x, min.y}, {glyph->uvMax.x, glyph->uvMin.y}, textColor});
        vertices.push_back({{max.x, max.y}, {glyph->uvMax.x, glyph->uvMax.y}, textColor});
        vertices.push_back({{min.x, max.y}, {glyph->uvMin.x, glyph->uvMax.y}, textColor});
        indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});

        pen.x += glyph->advance;
    }

    indexCount_ = static_cast<std::uint32_t>(indices.size());
    if (vertices.empty() || indices.empty()) {
        return;
    }

    LveBuffer vertexStaging{
        device,
        sizeof(TextVertex),
        static_cast<std::uint32_t>(vertices.size()),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    vertexStaging.map();
    vertexStaging.writeToBuffer(vertices.data());

    vertexBuffer_ = std::make_unique<LveBuffer>(
        device,
        sizeof(TextVertex),
        static_cast<std::uint32_t>(vertices.size()),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    device.copyBuffer(vertexStaging.getBuffer(), vertexBuffer_->getBuffer(), sizeof(TextVertex) * vertices.size());

    LveBuffer indexStaging{
        device,
        sizeof(std::uint32_t),
        indexCount_,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    indexStaging.map();
    indexStaging.writeToBuffer(indices.data());

    indexBuffer_ = std::make_unique<LveBuffer>(
        device,
        sizeof(std::uint32_t),
        indexCount_,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    device.copyBuffer(indexStaging.getBuffer(), indexBuffer_->getBuffer(), sizeof(std::uint32_t) * indices.size());
}

void TextMesh::bind(VkCommandBuffer commandBuffer) const {
    if (vertexBuffer_ == nullptr || indexBuffer_ == nullptr) {
        return;
    }

    const VkBuffer buffers[] = {vertexBuffer_->getBuffer()};
    constexpr VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer_->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

void TextMesh::draw(VkCommandBuffer commandBuffer) const {
    if (indexCount_ == 0) {
        return;
    }

    vkCmdDrawIndexed(commandBuffer, indexCount_, 1, 0, 0, 0);
}

} // namespace lve
