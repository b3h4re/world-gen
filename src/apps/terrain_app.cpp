#include "terrain_app.hpp"

#include "game/input/input_system.hpp"
#include "game/input/qt_input_reader.hpp"
#include "model/buffer/lve_buffer.hpp"
#include "renderer/systems/loading_overlay_system.hpp"
#include "renderer/systems/terrain_render_system.hpp"
#include "files/exporter.hpp"

#include <algorithm>
#include <iostream>
#include <chrono>
#include <random>
#include <utility>
#include <vulkan/vulkan.h>

namespace lve {

TerrainApp::TerrainApp() : TerrainApp(wgen::AppConfig{}) {}

TerrainApp::TerrainApp(const wgen::AppConfig& config)
    : config_{config}, core_{config_}, renderer_{config_.windowConfig},
      gui_{
          renderer_.window().controlsWidget(),
          Callbacks{
              .regenerateTerrain = [this] { regenerateWithRandomSeed(); },
              .reloadTerrain = [this] { reloadConfiguredSeed(); },
              .switchColor = [this] {
                core_.rotateColorFunction();
                renderer_.colorMapper().uploadNewPixels(core_.getActiveColorFuncID());
              },
              .pipelineChanged = [this](wgen::GeneratorPipelineSpec pipeline) {
                  core_.setPipeline(std::move(pipeline));
              },
              .planetPipelineChanged = [this](wgen::Generator3dPipelineSpec pipeline) {
                  core_.setPlanetPipeline(std::move(pipeline));
              },
              .planetShapeChanged = [this](std::size_t resolution, float radius) {
                  applyPlanetShape(resolution, radius);
              },
              .maximumPlanetPatchLevelChanged = [this](std::uint8_t level) {
                  core_.setMaximumPlanetPatchLevel(level);
              },
              .planetLodTransitionTimeScaleChanged = [this](double timeScale) {
                  config_.planetConfig.lodTransitionTimeScale = timeScale;
                  core_.setPlanetLodTransitionTimeScale(timeScale);
              },
              .currentPipeline = [this] {
                  return core_.currentPipeline();
              },
              .currentPlanetPipeline = [this] {
                  return core_.currentPlanetPipeline();
              },
              .getConfig = [this] {
                  return this->getConfig();
              },
              .configChanged = [this](wgen::WindowConfig config) {
                this->applyWindowConfig(config);
              },
              .exportTerrain = [this](ExportConfig cfg) {
                exporter_.cfg() = cfg;
                exporter_.exportToFile(core_.activeHeightMap());
              }
          }
      },
      limiter_{config.windowConfig.fps_max}, exporter_{renderer_.colorMapper()} {
    renderer_.window().setRenderParent(gui_.vulkanWidget());
    core_.regenerateTerrain(core_.config().terrainConfig.seed, TerrainGenerationTarget::All);
}


void TerrainApp::applyWindowConfig(const wgen::WindowConfig& cfg) {
    config_.windowConfig = cfg;
    limiter_.settargetFps(cfg.fps_max);
    renderer_.setDesiredPresentMode(cfg.present_mode);
}

void TerrainApp::applyPlanetShape(std::size_t resolution, float radius) {
    core_.setPlanetShape(resolution, radius);
    config_.planetConfig.resolution = resolution;
    config_.planetConfig.radius = radius;
}

TerrainApp::~TerrainApp() {
    renderer_.shutdownVulkanResources();
    renderer_.window().detachRenderParent();
}

wgen::AppConfig TerrainApp::getConfig() const {
    return config_;
}

void TerrainApp::rotateCameraViews() {
    switch (renderMode_) {
        case TerrainRenderModes::FlatPicture:
            renderMode_ = TerrainRenderModes::PlaneMesh3D;
            return;
        case TerrainRenderModes::PlaneMesh3D:
            renderMode_ = TerrainRenderModes::PlanetView;
            return;
        case TerrainRenderModes::PlanetView:
            renderMode_ = TerrainRenderModes::FlatPicture;
            return;
    }
}

void TerrainApp::updateCamerasStatus(std::vector<std::pair<CameraUpdateTarget, TerrainRenderModes>>& cameraTargets) {
    for (auto& pair : cameraTargets) {
        pair.first.active = pair.second == renderMode_;
    }
}

void TerrainApp::run() {
    auto& device = renderer_.device();
    auto& lveRenderer = renderer_.renderer();
    auto& window = renderer_.window();
    auto& colorMapper = renderer_.colorMapper();


    // ubo buffers
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
                               .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                               .build();
    auto imageInfo = colorMapper.descriptorInfo();

    std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (std::size_t i = 0; i < globalDescriptorSets.size(); ++i) {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        LveDescriptorWriter(*globalSetLayout, renderer_.descriptorPool())
            .writeBuffer(0, &bufferInfo)
            .writeImage(1, &imageInfo)
            .build(globalDescriptorSets[i]);
    }

    TerrainRenderSystem terrainRenderSystem{device, lveRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
    LoadingOverlaySystem loadingOverlaySystem{device, lveRenderer.getSwapChainRenderPass(), renderer_.descriptorPool()};
    Camera2d camera2d{};
    Camera3d camera3d{};
    Camera3d cameraPlanet{};
    AppInputSystem appInputSystem{};
    QtInputReader inputReader{window.qWindow(), window.rootWidget(), window.renderParentWidget(), window.renderWidget()};
    std::vector<std::pair<CameraUpdateTarget, TerrainRenderModes>> cameraTargets{
        {makeCameraTarget(camera2d, renderMode_ == TerrainRenderModes::FlatPicture), TerrainRenderModes::FlatPicture},
        {makeCameraTarget(camera3d, renderMode_ == TerrainRenderModes::PlaneMesh3D), TerrainRenderModes::PlaneMesh3D},
        {makeCameraTarget(cameraPlanet, renderMode_ == TerrainRenderModes::PlanetView), TerrainRenderModes::PlanetView}
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
            rotateCameraViews();
        }

        const auto currentTime = std::chrono::steady_clock::now();
        const float frameTime = std::min(std::chrono::duration<float>(currentTime - previousTime).count(), 0.1F);
        previousTime = currentTime;

        updateCamerasStatus(cameraTargets);
        appInputSystem.updateCameras(input, frameTime, lveRenderer.getAspectRatio(), cameraTargets);
        if (renderMode_ == TerrainRenderModes::PlanetView) {
            core_.updatePlanetLod({
                .position = glm::dvec3{cameraPlanet.position()},
                .forward = glm::dvec3{cameraPlanet.forward()},
                .right = glm::dvec3{cameraPlanet.right()},
                .up = glm::dvec3{cameraPlanet.up()},
                .verticalFov = cameraPlanet.verticalFov(),
                .aspectRatio = cameraPlanet.aspectRatio(),
                .nearPlane = cameraPlanet.nearPlane(),
                .farPlane = cameraPlanet.farPlane(),
                .viewportHeight = lveRenderer.getViewportHeight(),
            }, frameTime);
        }

        if (const VkCommandBuffer commandBuffer = lveRenderer.beginFrame()) {
            const int frameIndex = lveRenderer.getFrameIndex();

            renderer_.clearRetiredObjects(frameIndex);
            applyFinishedTerrainJob(frameIndex);
            renderer_.setPlanetDrawPatches(core_.planetDrawPatchStates());

            FrameInfo frameInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera2d,
                camera3d,
                cameraPlanet,
                globalDescriptorSets[frameIndex],
                renderMode_,
                renderer_.objects2d(),
                renderer_.objects3d(),
                renderer_.objectsPlanet()
            };

            GlobalUbo ubo{};
            switch (renderMode_) {
                case TerrainRenderModes::FlatPicture: {
                    ubo.projection = camera2d.projectionView();
                    break;
                }
                case TerrainRenderModes::PlaneMesh3D: {
                    ubo.projection = camera3d.projection();
                    ubo.view = camera3d.view();
                    ubo.inverseView = glm::inverse(camera3d.view());
                    break;
                }
                case TerrainRenderModes::PlanetView: {
                    ubo.projection = cameraPlanet.projection();
                    ubo.view = cameraPlanet.view();
                    ubo.inverseView = glm::inverse(cameraPlanet.view());
                    const wgen::TerrainDisplayHeightRange displayRange =
                        core_.activePlanetDisplayHeightRange();
                    ubo.heightParams = {
                        displayRange.minimumMeters,
                        displayRange.maximumMeters,
                    };
                    break;
                }
            }
            uboBuffers[frameIndex]->writeToBuffer(&ubo);
            uboBuffers[frameIndex]->flush();

            lveRenderer.beginSwapChainRenderPass(commandBuffer);
            terrainRenderSystem.render(frameInfo);
            if (core_.isBlockingTerrainJobRunning()) {
                loadingOverlaySystem.render(commandBuffer, frameTime);
            }
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

    if (result->terrainMesh) {
        renderer_.applyTerrainMesh(frameIndex, std::move(*result->terrainMesh));
    }
    if (result->planetBatch) {
        renderer_.applyPlanetPatchBatch(frameIndex, std::move(*result->planetBatch));
    }
}

TerrainGenerationTarget TerrainApp::currentGenerationTarget() const {
    switch (renderMode_) {
        case TerrainRenderModes::FlatPicture:
        case TerrainRenderModes::PlaneMesh3D:
            return TerrainGenerationTarget::Terrain;
        case TerrainRenderModes::PlanetView:
            return TerrainGenerationTarget::Planet;
    }

    return TerrainGenerationTarget::All;
}

void TerrainApp::regenerateWithRandomSeed() {
    std::random_device rd;
    core_.regenerateTerrain(rd(), currentGenerationTarget());
}

void TerrainApp::reloadConfiguredSeed() {
    core_.regenerateTerrain(core_.config().terrainConfig.seed, currentGenerationTarget());
}

} // namespace lve
