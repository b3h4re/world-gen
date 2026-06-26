#include "terrain_app.hpp"

#include "game/2d/camera/camera_2d.hpp"
#include "game/2d/input/camera_controller_2d.hpp"
#include "game/2d/rendering/render_system_2d.hpp"
#include "terrain/terrain.hpp"
#include "terrain/generators/generators.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <random>
#include <iostream>

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

}

TerrainApp::TerrainApp() : TerrainApp(wgen::AppConfig{}) {}

TerrainApp::TerrainApp(const wgen::AppConfig &config) : config{config} {
    std::uint32_t seed;
    if (config.terrainConfig.setSeed) {
        seed = config.terrainConfig.seed;
    } else {
        std::random_device rd;
        seed = rd();
    }
    generators.push_back(std::make_unique<wgen::DLADualFilterBlur>(wgen::DLADualFilterBlur(
        3,
        seed,
        wgen::defaultDLAHeightFunction<0.15F>
    )));
    // generators.push_back(std::make_unique<wgen::DLABasic>(wgen::DLABasic(
    //     config.terrainConfig.dla.numSteps,
    //     seed,
    //     wgen::defaultDLAHeightFunction<0.15F>
    // )));
    // generators.push_back(std::make_unique<wgen::DLABasic>(wgen::DLABasic(
    //     config.terrainConfig.dla.numSteps,
    //     seed,
    //     wgen::defaultDLAHeightFunction<0.15F>
    // )));
    // generators.push_back(std::make_unique<wgen::WorleyNoise2d>(wgen::WorleyNoise2d(
    //     config.terrainConfig.worley.gridWidth,
    //     config.terrainConfig.worley.gridHeight,
    //     config.terrainConfig.worley.dotsPerCell,
    //     seed,
    //     config.terrainConfig.worley.p,
    //     config.terrainConfig.worley.numPoints
    // )));
    // generators.push_back(std::make_unique<wgen::WaveletNoise2d>(wgen::WaveletNoise2d(
    //     config.terrainConfig.wavelet.gridWidth,
    //     config.terrainConfig.wavelet.gridHeight,
    //     seed,
    //     wgen::defaultReconstructionKernel
    // )));
    // generators.push_back(std::make_unique<wgen::SimplexNoise2d>(wgen::SimplexNoise2d(
    //     config.terrainConfig.simplex.gridWidth,
    //     config.terrainConfig.simplex.gridHeight,
    //     config.terrainConfig.simplex.dotsPerCell,
    //     seed
    // )));
    // generators.push_back(std::make_unique<wgen::PerlinNoise2d>(wgen::PerlinNoise2d(
    //     config.terrainConfig.perlin.gridWidth,
    //     config.terrainConfig.perlin.gridHeight,
    //     config.terrainConfig.perlin.dotsPerCell,
    //     seed,
    //     wgen::defaultPerlinInterp
    // )));
    // generators.push_back(std::make_unique<wgen::ValueNoiseGenerator>(wgen::ValueNoiseGenerator(seed)));
    // generators.push_back(std::make_unique<wgen::LayeredSinNoiseGenerator>(wgen::LayeredSinNoiseGenerator(seed)));

    loadTerrain();
}

void TerrainApp::loadTerrain() {
    std::size_t width = config.terrainConfig.width;
    std::size_t height = config.terrainConfig.height;
    auto heightMap = generators[used_generator]->generateHeightMap(width, height).normal();

    std::vector<Vertex2d> vertices;
    std::vector<std::uint32_t> indices;
    appendHeightMapMesh(heightMap, -0.9F, 0.9F, -0.9F, 0.9F, vertices, indices, wgen::terrainColor);

    auto mesh = std::make_shared<Mesh2d>(device_, vertices, indices);
    objects_.push_back({std::move(mesh), {}});
}

void TerrainApp::run() {
    RenderSystem2d renderSystem{device_, renderer_.getSwapChainRenderPass()};
    Camera2d camera{};
    CameraController2d cameraController{};
    auto previousTime = std::chrono::steady_clock::now();

    while (!window_.shouldClose()) {
        glfwPollEvents();
        if (glfwGetKey(window_.getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window_.getGLFWwindow(), GLFW_TRUE);
        }

        const auto currentTime = std::chrono::steady_clock::now();
        const float frameTime = std::min(
            std::chrono::duration<float>(currentTime - previousTime).count(),
            0.1F);
        previousTime = currentTime;

        camera.setAspectRatio(renderer_.getAspectRatio());
        cameraController.update(window_.getGLFWwindow(), frameTime, camera);

        if (const VkCommandBuffer commandBuffer = renderer_.beginFrame()) {
            renderer_.beginSwapChainRenderPass(commandBuffer);
            renderSystem.render(commandBuffer, camera, objects_);
            renderer_.endSwapChainRenderPass(commandBuffer);
            renderer_.endFrame();
        }
    }

    vkDeviceWaitIdle(device_.device());
}

} // namespace lve
