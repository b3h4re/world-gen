#include "terrain_app.hpp"

#include "game/input/input_system.hpp"
#include "game/input/qt_input_reader.hpp"
#include "model/buffer/lve_buffer.hpp"
#include "renderer/systems/terrain_render_system.hpp"

#include <algorithm>
#include <chrono>
#include <random>
#include <vulkan/vulkan.h>

namespace lve {

TerrainApp::TerrainApp() : TerrainApp(wgen::AppConfig{}) {}

TerrainApp::TerrainApp(const wgen::AppConfig& config)
    : core_{config},
      renderer_{config.windowConfig},
      gui_{
          renderer_.window().controlsWidget(),
          TerrainAppGui::Callbacks{
              .regenerateTerrain = [this] { regenerateWithRandomSeed(); },
              .reloadTerrain = [this] { reloadConfiguredSeed(); },
              .switchColor = [this] { core_.rotateColorFunction(); },
          }
      } {
    renderer_.window().setRenderParent(gui_.vulkanWidget());
    renderer_.setTerrainMesh(core_.loadTerrain());
}

void TerrainApp::run() {
    auto& device = renderer_.device();
    auto& lveRenderer = renderer_.renderer();
    auto& window = renderer_.window();

    std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (std::size_t i = 0; i < uboBuffers.size(); ++i) {
        uboBuffers[i] = std::make_unique<LveBuffer>(
            device,
            sizeof(GlobalUbo),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        uboBuffers[i]->map();
    }

    auto globalSetLayout = LveDescriptorSetLayout::Builder(device)
                               .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                               .build();

    std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (std::size_t i = 0; i < globalDescriptorSets.size(); ++i) {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        LveDescriptorWriter(*globalSetLayout, renderer_.descriptorPool())
            .writeBuffer(0, &bufferInfo)
            .build(globalDescriptorSets[i]);
    }

    TerrainRenderSystem terrainRenderSystem{device, lveRenderer.getSwapChainRenderPass()};
    Camera2d camera2d{};
    Camera3d camera3d{};
    AppInputSystem appInputSystem{};
    QtInputReader inputReader{window.qWindow(), window.rootWidget(), window.renderWidget()};
    std::vector<CameraUpdateTarget> cameraTargets{
        makeCameraTarget(camera2d, !render3d_),
        makeCameraTarget(camera3d, render3d_),
    };

    auto previousTime = std::chrono::steady_clock::now();

    while (!window.shouldClose()) {
        window.pollEvents();
        if (window.shouldClose()) {
            break;
        }

        const VkExtent2D windowExtent = window.getExtent();
        const AppInputState input = inputReader.readInputState(windowExtent);

        if (input.escapeJustPressed) {
            window.requestClose();
            break;
        }

        if (input.viewToggleJustPressed) {
            render3d_ = !render3d_;
        }

        const auto currentTime = std::chrono::steady_clock::now();
        const float frameTime = std::min(std::chrono::duration<float>(currentTime - previousTime).count(), 0.1F);
        previousTime = currentTime;

        cameraTargets[0].active = !render3d_;
        cameraTargets[1].active = render3d_;
        appInputSystem.updateCameras(input, frameTime, lveRenderer.getAspectRatio(), cameraTargets);

        if (const VkCommandBuffer commandBuffer = lveRenderer.beginFrame()) {
            const int frameIndex = lveRenderer.getFrameIndex();

            renderer_.clearRetiredObjects(frameIndex);
            applyFinishedTerrainJob(frameIndex);

            FrameInfo frameInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera2d,
                camera3d,
                globalDescriptorSets[frameIndex],
                render3d_,
                renderer_.objects2d(),
                renderer_.objects3d(),
            };

            GlobalUbo ubo{};
            if (render3d_) {
                ubo.projection = camera3d.projection();
                ubo.view = camera3d.view();
                ubo.inverseView = glm::inverse(camera3d.view());
            } else {
                ubo.projection = camera2d.projectionView();
            }
            uboBuffers[frameIndex]->writeToBuffer(&ubo);
            uboBuffers[frameIndex]->flush();

            lveRenderer.beginSwapChainRenderPass(commandBuffer);
            terrainRenderSystem.render(frameInfo);
            lveRenderer.endSwapChainRenderPass(commandBuffer);
            lveRenderer.endFrame();
        }
    }

    renderer_.waitIdle();
}

void TerrainApp::applyFinishedTerrainJob(int frameIndex) {
    auto result = core_.tryTakeFinishedTerrainJob();
    if (!result) {
        return;
    }

    renderer_.applyTerrainMesh(frameIndex, std::move(result->data));
}

void TerrainApp::regenerateWithRandomSeed() {
    std::random_device rd;
    core_.regenerateTerrain(rd());
}

void TerrainApp::reloadConfiguredSeed() {
    core_.regenerateTerrain(core_.config().terrainConfig.seed);
}

} // namespace lve
