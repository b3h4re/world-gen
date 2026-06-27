#pragma once

#include "game/2d/camera/camera_2d.hpp"
#include "game/3d/camera/camera_3d.hpp"


#include <vulkan/vulkan.h>


namespace lve {
    struct GlobalUbo {
        glm::mat4 projection{1.0f};
        glm::mat4 view{1.0f};
        glm::mat4 inverseView{1.0f};
    };

    struct FrameInfo {
        int frameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        Camera2d &camera2d;
        Camera3d &camera3d;
        VkDescriptorSet globalDescriptorSet;
        // ObjectControlSystem &objectControl;
    };
}
