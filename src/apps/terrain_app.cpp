#include "terrain_app.hpp"

#include "game/2d/camera/camera_2d.hpp"
#include "game/2d/input/camera_controller_2d.hpp"
#include "game/2d/rendering/render_system_2d.hpp"
#include "game/3d/camera/camera_3d.hpp"
#include "game/3d/input/camera_controller_3d.hpp"
#include "game/3d/rendering/render_system_3d.hpp"
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
            const float zPos = -1.0F + 2.0F * static_cast<float>(y) / static_cast<float>(height - 1);
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
    std::uint32_t seed;
    if (config.terrainConfig.setSeed) {
        seed = config.terrainConfig.seed;
    } else {
        std::random_device rd;
        seed = rd();
    }
    generators.push_back(std::make_unique<wgen::DLADualFilterBlur>(wgen::DLADualFilterBlur(
        config.terrainConfig.dla.numSteps,
        seed,
        wgen::defaultDLAHeightFunction(config.terrainConfig.dla.heightFuncScale),
        config.terrainConfig.dla.fill,
        config.terrainConfig.dla.jiggle
    )));
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
    appendHeightMapMesh(heightMap, -0.9F, 0.9F, -0.9F, 0.9F, vertices, indices, wgen::terrainBlackAndWhite);

    auto mesh = std::make_shared<Mesh2d>(device_, vertices, indices);
    objects2d_.push_back({std::move(mesh), {}});

    std::vector<Vertex3d> vertices3d;
    std::vector<std::uint32_t> indices3d;
    appendHeightMapMesh3d(heightMap, vertices3d, indices3d);

    auto mesh3d = std::make_shared<Mesh3d>(device_, vertices3d, indices3d);
    objects3d_.push_back({std::move(mesh3d), {}});
}

void TerrainApp::run() {
    RenderSystem2d renderSystem2d{device_, renderer_.getSwapChainRenderPass()};
    RenderSystem3d renderSystem3d{device_, renderer_.getSwapChainRenderPass()};
    Camera2d camera2d{};
    Camera3d camera3d{};
    CameraController2d cameraController2d{};
    CameraController3d cameraController3d{};
    bool viewToggleWasPressed{false};
    auto previousTime = std::chrono::steady_clock::now();

    while (!window_.shouldClose()) {
        glfwPollEvents();
        if (glfwGetKey(window_.getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window_.getGLFWwindow(), GLFW_TRUE);
        }
        const bool viewToggleIsPressed = glfwGetKey(window_.getGLFWwindow(), GLFW_KEY_V) == GLFW_PRESS;
        if (viewToggleIsPressed && !viewToggleWasPressed) {
            render3d_ = !render3d_;
        }
        viewToggleWasPressed = viewToggleIsPressed;

        const auto currentTime = std::chrono::steady_clock::now();
        const float frameTime = std::min(
            std::chrono::duration<float>(currentTime - previousTime).count(),
            0.1F);
        previousTime = currentTime;

        if (render3d_) {
            camera3d.setPerspectiveProjection(glm::radians(50.0F), renderer_.getAspectRatio(), 0.1F, 20.0F);
            cameraController3d.update(window_.getGLFWwindow(), frameTime, camera3d);
        } else {
            camera2d.setAspectRatio(renderer_.getAspectRatio());
            cameraController2d.update(window_.getGLFWwindow(), frameTime, camera2d);
        }

        if (const VkCommandBuffer commandBuffer = renderer_.beginFrame()) {
            renderer_.beginSwapChainRenderPass(commandBuffer);
            if (render3d_) {
                renderSystem3d.render(commandBuffer, camera3d, objects3d_);
            } else {
                renderSystem2d.render(commandBuffer, camera2d, objects2d_);
            }
            renderer_.endSwapChainRenderPass(commandBuffer);
            renderer_.endFrame();
        }
    }

    vkDeviceWaitIdle(device_.device());
}

} // namespace lve
