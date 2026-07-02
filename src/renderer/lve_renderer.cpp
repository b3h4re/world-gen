#include "lve_renderer.hpp"


#include <stdexcept>
#include <array>


namespace lve {

    LveRenderer::LveRenderer(WindowSurface& window, LveDevice& device, PresentMode desiredPresentMode)
            : lveWindow(window), lveDevice(device), desiredPresentMode_{desiredPresentMode} {
        recreateSwapChain();
        createCommandBuffers();
    }

    LveRenderer::~LveRenderer() {
        destroySwapChain();
        freeCommandBuffers();
    }

    void LveRenderer::destroySwapChain() {
        if (isFrameStarted) {
            abortFrame();
        }

        if (lveSwapChain != nullptr) {
            vkDeviceWaitIdle(lveDevice.device());
            lveSwapChain.reset();
        }
    }

    void LveRenderer::recreateSwapChain() {
        auto extent = lveWindow.getExtent();
        while (extent.width == 0 || extent.height == 0) {
            extent = lveWindow.getExtent();
            lveWindow.waitEvents();
        }

        vkDeviceWaitIdle(lveDevice.device());
        isSwapChainDirty = false;

        if (lveSwapChain == nullptr) {
            lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent, desiredPresentMode_);
        } else {
            std::shared_ptr<LveSwapChain> oldSwapChain = std::move(lveSwapChain);

            lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent, oldSwapChain, desiredPresentMode_);

            if (!oldSwapChain->compareSwapFormats(*lveSwapChain.get())) {
                throw std::runtime_error("Swap chain image format has change");
            }
        }

        // TODO soon
    }

    void LveRenderer::setDesiredPresentMode(PresentMode desiredPresentMode) {
        desiredPresentMode_ = desiredPresentMode;
        isSwapChainDirty = true;
    }
    PresentMode LveRenderer::getDesiredPresentMode() const {
        return desiredPresentMode_;
    }

    void LveRenderer::createCommandBuffers() {
        commandBuffers.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = lveDevice.getCommandPool();
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        if (vkAllocateCommandBuffers(lveDevice.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers");
        }
    }


    void LveRenderer::freeCommandBuffers() {
        vkFreeCommandBuffers(
            lveDevice.device(),
            lveDevice.getCommandPool(),
            static_cast<uint32_t>(commandBuffers.size()),
            commandBuffers.data()
        );
        commandBuffers.clear();
    }


    VkCommandBuffer LveRenderer::beginFrame() {
        assert(!isFrameStarted && "Can't call begin frame while frame is in progress.");
        if (isSwapChainDirty) {
            recreateSwapChain();
            return nullptr;
        }

        auto result = lveSwapChain->acquireNextImage(&currentImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return nullptr;
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image");
        }

        isFrameStarted = true;

        auto commandBuffer = getCurrentCommandBuffer();

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer");
        }
        return commandBuffer;
    }
    void LveRenderer::endFrame() {
        assert(isFrameStarted && "Can't end frame while frame is not in progress");
        if (lveWindow.shouldClose()) {
            abortFrame();
            return;
        }
        auto commandBuffer = getCurrentCommandBuffer();

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

        auto result = lveSwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || lveWindow.wasWindowResized()) {
            lveWindow.resetWindowResizedFlag();
            recreateSwapChain();
        } else if (result != VK_SUCCESS){
            throw std::runtime_error("failed to acquire swap chain image");
        }

        isFrameStarted = false;
        currentFrameIndex = (currentFrameIndex + 1) % LveSwapChain::MAX_FRAMES_IN_FLIGHT;
    }
    void LveRenderer::abortFrame() {
        if (!isFrameStarted) {
            return;
        }

        const VkCommandBuffer commandBuffer = getCurrentCommandBuffer();

        vkResetCommandBuffer(commandBuffer, 0);

        isFrameStarted = false;
    }


    void LveRenderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer) {
        assert(isFrameStarted && "Can't start swap chain render pass when frame is not in progress");
        assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin swap chain render pass on command buffer from different frame");

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = lveSwapChain->getRenderPass();
        renderPassInfo.framebuffer = lveSwapChain->getFrameBuffer(currentImageIndex);

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = lveSwapChain->getSwapChainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(lveSwapChain->getSwapChainExtent().width);
        viewport.height = static_cast<float>(lveSwapChain->getSwapChainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{{0, 0}, lveSwapChain->getSwapChainExtent()};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }
    void LveRenderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer) {
        assert(isFrameStarted && "Can't end swap chain render pass when frame is not in progress");
        assert(commandBuffer == getCurrentCommandBuffer() && "Can't end swap chain render pass on command buffer from different frame");

        vkCmdEndRenderPass(commandBuffer);
    }
}
