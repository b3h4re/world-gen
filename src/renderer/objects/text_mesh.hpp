#pragma once

#include "stb/font_atlas.hpp"
#include "device/lve_device.hpp"
#include "model/buffer/lve_buffer.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>


namespace lve {

    struct TextVertex {
        glm::vec2 position;
        glm::vec2 uv;
        glm::vec3 color;

        static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    };

    class TextMesh {
        public:
            TextMesh(LveDevice& device, FontAtlas& font, std::string_view text, glm::vec2 position);

            TextMesh(const TextMesh &) = delete;
            TextMesh &operator=(const TextMesh &) = delete;

            void bind(VkCommandBuffer commandBuffer) const;
            void draw(VkCommandBuffer commandBuffer) const;
        private:
            std::unique_ptr<LveBuffer> vertexBuffer_;
            std::unique_ptr<LveBuffer> indexBuffer_;
            std::uint32_t indexCount_{};
    };
} // namespace lve
