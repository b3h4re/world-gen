#pragma once
#include "device/lve_device.hpp"
#include "swap_chain/lve_swap_chain.hpp"
#include "window/window_surface.hpp"

#include <cassert>
#include <vector>
#include <memory>


namespace lve {

    class LveRenderer {
        public:

            LveRenderer(WindowSurface& window, LveDevice& device, PresentMode desiredPresentMode = PresentMode::VSync);
            ~LveRenderer();

            LveRenderer(const LveRenderer&) = delete;
            LveRenderer &operator=(const LveRenderer&) = delete;


            bool isFrameInProgress() const { return isFrameStarted; }

            VkRenderPass getSwapChainRenderPass() const { return lveSwapChain->getRenderPass(); }
            float getAspectRatio() const { return lveSwapChain->extentAspectRatio(); }
            uint32_t getViewportHeight() const { return lveSwapChain->height(); }

            VkCommandBuffer getCurrentCommandBuffer() const {
                assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
                return commandBuffers[currentFrameIndex];
            }

            int getFrameIndex() const {
                assert(isFrameStarted && "Cannot get frame index when frame not in progress");
                return currentFrameIndex;
            }

            VkCommandBuffer beginFrame();
            void endFrame();
            void abortFrame();

            void setDesiredPresentMode(PresentMode desiredPresentMode);
            PresentMode getDesiredPresentMode() const;


            void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
            void endSwapChainRenderPass(VkCommandBuffer commandBuffer);
            void destroySwapChain();

        private:
            void createCommandBuffers();
            void freeCommandBuffers();
            void recreateSwapChain();
            PresentMode desiredPresentMode_{PresentMode::VSync};

            WindowSurface& lveWindow;
            LveDevice& lveDevice;
            std::unique_ptr<LveSwapChain> lveSwapChain;
            std::vector<VkCommandBuffer> commandBuffers;

            uint32_t currentImageIndex;
            int currentFrameIndex{0};
            bool isFrameStarted{false};
            bool isSwapChainDirty{false};

    };

}
