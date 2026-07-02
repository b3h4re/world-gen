#include "terrain_app.hpp"

#include "game/input/input_system.hpp"
#include "game/input/qt_input_reader.hpp"
#include "model/buffer/lve_buffer.hpp"
#include "renderer/systems/terrain_render_system.hpp"

#include <algorithm>
#include <chrono>
#include <random>
#include <utility>
#include <vulkan/vulkan.h>

namespace lve {

TerrainApp::TerrainApp() : TerrainApp(wgen::AppConfig{}) {}

TerrainApp::TerrainApp(const wgen::AppConfig& config)
    : core_{config}, config_{config},
      renderer_{config.windowConfig}, limiter_{config.windowConfig.fps_max},
      gui_{
          renderer_.window().controlsWidget(),
          Callbacks{
              .regenerateTerrain = [this] { regenerateWithRandomSeed(); },
              .reloadTerrain = [this] { reloadConfiguredSeed(); },
              .switchColor = [this] { core_.rotateColorFunction(); },
              .pipelineChanged = [this](wgen::GeneratorPipelineSpec pipeline) {
                  core_.setPipeline(std::move(pipeline));
              },
              .currentPipeline = [this] {
                  return core_.currentPipeline();
              },
              .getConfig = [this] {
                  return this->getConfig();
              },
              .configChanged = [this](wgen::WindowConfig config) {
                this->applyWindowConfig(config);
              }
          }
      } {
    renderer_.window().setRenderParent(gui_.vulkanWidget());
    renderer_.setTerrainMesh(core_.loadTerrain());
}


void TerrainApp::applyWindowConfig(const wgen::WindowConfig& cfg) {
    config_.windowConfig = cfg;
    limiter_.settargetFps(cfg.fps_max);
    renderer_.setDesiredPresentMode(cfg.present_mode);
}

TerrainApp::~TerrainApp() {
    renderer_.shutdownVulkanResources();
    renderer_.window().detachRenderParent();
}

wgen::AppConfig TerrainApp::getConfig() const {
    return config_;
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
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            device.properties.limits.minUniformBufferOffsetAlignment
        );
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

    TerrainRenderSystem terrainRenderSystem{device, lveRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
    Camera2d camera2d{};
    Camera3d camera3d{};
    AppInputSystem appInputSystem{};
    QtInputReader inputReader{window.qWindow(), window.rootWidget(), window.renderParentWidget(), window.renderWidget()};
    std::vector<CameraUpdateTarget> cameraTargets{
        makeCameraTarget(camera2d, !render3d_),
        makeCameraTarget(camera3d, render3d_),
    };

    auto previousTime = std::chrono::steady_clock::now();

    while (!window.shouldClose()) {
        window.pollEvents();
        limiter_.wait();
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
            ubo.colorFuncID = core_.getActiveColorFuncID();
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
            if (window.shouldClose()) {
                lveRenderer.abortFrame();
                break;
            }
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
