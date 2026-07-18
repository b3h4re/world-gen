#pragma once

#include "game/camera/camera_2d.hpp"
#include "game/camera/camera_3d.hpp"
#include "game/objects/game_object_2d.hpp"
#include "game/objects/game_object_3d.hpp"
#include "renderer/objects/local_clipmap_render_resources.hpp"

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace lve {
    struct GlobalUbo {
        glm::mat4 projection{1.0F};
        glm::mat4 view{1.0F};
        glm::mat4 inverseView{1.0F};

        alignas(16) glm::vec2 heightParams{-1.0F, 1.0F}; // {min height, max height}
        alignas(16) glm::vec4 localFrameEastRadius{};
        alignas(16) glm::vec4 localFrameNorth{};
        alignas(16) glm::vec4 localFrameUp{};
        // {center x, center y, inner half extent, outer half extent}, meters.
        alignas(16) glm::vec4 localCoverage{};
        // {hybrid enabled, flatten amount, flat extent, curved extent}.
        alignas(16) glm::vec4 hybridParams{};
    };

    static_assert(offsetof(GlobalUbo, heightParams) == 192);
    static_assert(offsetof(GlobalUbo, localFrameEastRadius) == 208);
    static_assert(offsetof(GlobalUbo, localFrameNorth) == 224);
    static_assert(offsetof(GlobalUbo, localFrameUp) == 240);
    static_assert(offsetof(GlobalUbo, localCoverage) == 256);
    static_assert(offsetof(GlobalUbo, hybridParams) == 272);
    static_assert(sizeof(GlobalUbo) == 288);

    struct TextInfo {
        std::string_view text;
        glm::vec2 position{};
        glm::vec3 color{1.0F, 1.0F, 1.0F};
        float scale{1.0F};
    };


    enum class TerrainRenderModes {
        FlatPicture,
        PlaneMesh3D,
        PlanetView,
        LocalClipmapDebug,
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
        const std::vector<LocalClipmapRenderObject> &objectsLocalClipmap;
        bool localClipmapCoverageActive;
    };
}
