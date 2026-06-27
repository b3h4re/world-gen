#include "terrain_app.hpp"

#include "game/input/input_system.hpp"
#include "model/buffer/lve_buffer.hpp"
#include "renderer/systems/terrain_render_system.hpp"
#include "renderer/systems/text_render_system.hpp"
#include "stb/font_atlas.hpp"
#include "terrain/generators/generators.hpp"
#include "terrain/terrain.hpp"

#include <algorithm>
#include <chrono>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

namespace lve {

namespace {

void appendHeightMapMesh(
        const wgen::HeightMap<float>& heightMap,
        float minX,
        float maxX,
        float minY,
        float maxY,
        std::vector<Vertex2d>& vertices,
        std::vector<std::uint32_t>& indices,
        wgen::colorFromHeightFunc colorFunc = wgen::terrainColor) {
    const std::size_t width = heightMap.width();
    const std::size_t height = heightMap.height();

    vertices.reserve(vertices.size() + width * height * 4);
    indices.reserve(indices.size() + width * height * 6);

    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const float left = minX + (maxX - minX) * static_cast<float>(x) / static_cast<float>(width - 1);
            const float right = minX + (maxX - minX) * static_cast<float>(x + 1) / static_cast<float>(width - 1);
            const float top = minY + (maxY - minY) * static_cast<float>(y) / static_cast<float>(height - 1);
            const float bottom = minY + (maxY - minY) * static_cast<float>(y + 1) / static_cast<float>(height - 1);
            const float sample = heightMap.at(x, y);
            const glm::vec3 color = colorFunc(sample);
            const auto base = static_cast<std::uint32_t>(vertices.size());

            vertices.insert(
                vertices.end(),
                {{{left, top}, color}, {{right, top}, color}, {{right, bottom}, color}, {{left, bottom}, color}});
            indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
    }
}

void appendHeightMapMesh3d(
        const wgen::HeightMap<float>& heightMap,
        std::vector<Vertex3d>& vertices,
        std::vector<std::uint32_t>& indices,
        wgen::colorFromHeightFunc colorFunc = wgen::terrainBlackAndWhite) {
    const std::size_t width = heightMap.width();
    const std::size_t height = heightMap.height();

    vertices.reserve(vertices.size() + width * height);
    indices.reserve(indices.size() + (width - 1) * (height - 1) * 6);

    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const float xPos = -1.0F + 2.0F * static_cast<float>(x) / static_cast<float>(width - 1);
            const float zPos = 1.0F - 2.0F * static_cast<float>(y) / static_cast<float>(height - 1);
            const float sample = heightMap.at(x, y);
            vertices.push_back({
                {xPos, 0.1F * sample, zPos},
                colorFunc(sample)
            });
        }
    }

    for (std::size_t y = 0; y + 1 < height; ++y) {
        for (std::size_t x = 0; x + 1 < width; ++x) {
            const auto topLeft = static_cast<std::uint32_t>(y * width + x);
            const auto topRight = static_cast<std::uint32_t>(y * width + x + 1);
            const auto bottomLeft = static_cast<std::uint32_t>((y + 1) * width + x);
            const auto bottomRight = static_cast<std::uint32_t>((y + 1) * width + x + 1);

            indices.insert(
                indices.end(),
                {topLeft, topRight, bottomRight, topLeft, bottomRight, bottomLeft});
        }
    }
}

}

TerrainApp::TerrainApp() : TerrainApp(wgen::AppConfig{}) {}

TerrainApp::TerrainApp(const wgen::AppConfig &config) : config{config} {
    initDescriptorPool();
    initFontAtlas();
    initGenerators(config.terrainConfig);
    loadTerrain();
    initDropDownMenu();
}

void TerrainApp::initDescriptorPool() {
    globalPool_ = LveDescriptorPool::Builder(device_)
            .setMaxSets(100)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16)
            .build();
}

void TerrainApp::initFontAtlas() {
    fontAtlas_ = bakeFontAtlas("assets/fonts/Inter-Regular.ttf", 32.0F);
}

void TerrainApp::initDropDownMenu() {
    UiButton::Config regenerateTerrainButton = {
        .color = {0.25F, 0.25F, 0.30F},
        .text = "Regenerate",
        .onClick =
            [this] {
                std::random_device rd;
                regenerateTerrain(rd());
            },
    };

    UiButton::Config reloadTerrainButton = {
        .color = {0.25F, 0.25F, 0.30F},
        .text = "Reload seed",
        .onClick = [this] { regenerateTerrain(this->config.terrainConfig.seed); },
    };

    std::vector<UiButton::Config> buttons{regenerateTerrainButton, reloadTerrainButton};

    dropdownMenu_ = std::make_unique<DropdownMenu>(device_, buttons);
}

void TerrainApp::initGenerators(const wgen::TerrainConfig &terrainConfig) {
    generators.clear();
    std::uint32_t seed;
    if (terrainConfig.setSeed) {
        seed = terrainConfig.seed;
    } else {
        std::random_device rd;
        seed = rd();
    }
    generators.push_back(std::make_unique<wgen::DLADualFilterBlur>(wgen::DLADualFilterBlur(
        terrainConfig.dla.numSteps, seed, wgen::defaultDLAHeightFunction(terrainConfig.dla.heightFuncScale),
        terrainConfig.dla.fill, terrainConfig.dla.jiggle)));
    // generators.push_back(std::make_unique<wgen::WorleyNoise2d>(wgen::WorleyNoise2d(
    //     terrainConfig.worley.gridWidth,
    //     terrainConfig.worley.gridHeight,
    //     terrainConfig.worley.dotsPerCell,
    //     seed,
    //     terrainConfig.worley.p,
    //     terrainConfig.worley.numPoints
    // )));
    // generators.push_back(std::make_unique<wgen::WaveletNoise2d>(wgen::WaveletNoise2d(
    //     terrainConfig.wavelet.gridWidth,
    //     terrainConfig.wavelet.gridHeight,
    //     seed,
    //     wgen::defaultReconstructionKernel
    // )));
    // generators.push_back(std::make_unique<wgen::SimplexNoise2d>(wgen::SimplexNoise2d(
    //     terrainConfig.simplex.gridWidth,
    //     terrainConfig.simplex.gridHeight,
    //     terrainConfig.simplex.dotsPerCell,
    //     seed
    // )));
    // generators.push_back(std::make_unique<wgen::PerlinNoise2d>(wgen::PerlinNoise2d(
    //     terrainConfig.perlin.gridWidth,
    //     terrainConfig.perlin.gridHeight,
    //     terrainConfig.perlin.dotsPerCell,
    //     seed,
    //     wgen::defaultPerlinInterp
    // )));
    // generators.push_back(std::make_unique<wgen::ValueNoiseGenerator>(wgen::ValueNoiseGenerator(seed)));
    // generators.push_back(std::make_unique<wgen::LayeredSinNoiseGenerator>(wgen::LayeredSinNoiseGenerator(seed)));
}

void TerrainApp::regenerateTerrain(std::uint32_t seed) {
    config.terrainConfig.seed = seed;
    initGenerators(config.terrainConfig);
    loadTerrain();
}

void TerrainApp::loadTerrain() {
    objects2d_.clear();
    objects3d_.clear();
    std::size_t width = config.terrainConfig.width;
    std::size_t height = config.terrainConfig.height;
    auto heightMap = generators[used_generator]->generateHeightMap(width, height).normal();

    std::vector<Vertex2d> vertices;
    std::vector<std::uint32_t> indices;
    appendHeightMapMesh(heightMap, -1.0F, 1.0F, -1.0F, 1.0F, vertices, indices, wgen::terrainBlackAndWhite);

    auto mesh = std::make_shared<Mesh2d>(device_, vertices, indices);
    objects2d_.push_back({std::move(mesh), {}});

    std::vector<Vertex3d> vertices3d;
    std::vector<std::uint32_t> indices3d;
    appendHeightMapMesh3d(heightMap, vertices3d, indices3d);

    auto mesh3d = std::make_shared<Mesh3d>(device_, vertices3d, indices3d);
    objects3d_.push_back({std::move(mesh3d), {}});
}

void TerrainApp::run() {
    std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (std::size_t i = 0; i < uboBuffers.size(); ++i) {
        uboBuffers[i] = std::make_unique<LveBuffer>(
            device_,
            sizeof(GlobalUbo),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        uboBuffers[i]->map();
    }

    auto globalSetLayout = LveDescriptorSetLayout::Builder(device_)
                               .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                               .build();

    std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (std::size_t i = 0; i < globalDescriptorSets.size(); ++i) {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        LveDescriptorWriter(*globalSetLayout, *globalPool_)
            .writeBuffer(0, &bufferInfo)
            .build(globalDescriptorSets[i]);
    }

    TerrainRenderSystem terrainRenderSystem{device_, renderer_.getSwapChainRenderPass()};
    TextRenderSystem textRenderSystem{device_, renderer_.getSwapChainRenderPass(), *globalPool_, fontAtlas_};
    Camera2d camera2d{};
    Camera3d camera3d{};
    AppInputSystem appInputSystem{};
    AppInputState input{};
    std::vector<CameraUpdateTarget> cameraTargets{
        makeCameraTarget(camera2d, !render3d_),
        makeCameraTarget(camera3d, render3d_),
    };

    auto previousTime = std::chrono::steady_clock::now();

    while (!window_.shouldClose()) {
        glfwPollEvents();
        appInputSystem.updateInputState(window_.getGLFWwindow(), window_.getExtent(), input);

        const bool uiHandledInput = dropdownMenu_->update(input);
        if (input.escapeJustPressed && !uiHandledInput) {
            glfwSetWindowShouldClose(window_.getGLFWwindow(), GLFW_TRUE);
        }

        if (input.viewToggleJustPressed) {
            render3d_ = !render3d_;
        }

        const auto currentTime = std::chrono::steady_clock::now();
        const float frameTime = std::min(std::chrono::duration<float>(currentTime - previousTime).count(), 0.1F);
        previousTime = currentTime;

        cameraTargets[0].active = !render3d_;
        cameraTargets[1].active = render3d_;
        appInputSystem.updateCameras(input, frameTime, renderer_.getAspectRatio(), cameraTargets);

        if (const VkCommandBuffer commandBuffer = renderer_.beginFrame()) {
            const int frameIndex = renderer_.getFrameIndex();
            FrameInfo frameInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera2d,
                camera3d,
                globalDescriptorSets[frameIndex],
                render3d_,
                objects2d_,
                objects3d_,
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

            renderer_.beginSwapChainRenderPass(commandBuffer);
            terrainRenderSystem.render(frameInfo);
            dropdownMenu_->render(commandBuffer, terrainRenderSystem.renderSystem2d(), textRenderSystem);
            renderer_.endSwapChainRenderPass(commandBuffer);
            renderer_.endFrame();
        }
    }

    vkDeviceWaitIdle(device_.device());
}

} // namespace lve
