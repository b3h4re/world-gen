#pragma once

#include "game/camera/camera_2d.hpp"
#include "game/camera/camera_3d.hpp"
#include "game/objects/game_object_2d.hpp"
#include "game/objects/game_object_3d.hpp"

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace lve {
    struct GlobalUbo {
        glm::mat4 projection{1.0F};
        glm::mat4 view{1.0F};
        glm::mat4 inverseView{1.0F};

        alignas(16) glm::vec2 heightParams{-1.0F, 1.0F}; // {min height, max height}
    };

    struct TextInfo {
        std::string_view text;
        glm::vec2 position{};
        glm::vec3 color{1.0F, 1.0F, 1.0F};
        float scale{1.0F};
    };


    enum class TerrainRenderModes {
        FlatPicture,
        PlaneMesh3D,
        PlanetView
    };

    struct FrameInfo {
        int frameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        Camera2d &camera2d;
        Camera3d &camera3d;
        Camera3d &cameraPlanet;
        VkDescriptorSet globalDescriptorSet;
        TerrainRenderModes renderMode;
        const std::vector<GameObject2d> &objects2d;
        const std::vector<GameObject3d> &objects3d;
        const std::vector<GameObject3d> &objectsPlanet;
    };
}
