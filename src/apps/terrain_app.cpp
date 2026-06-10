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

namespace lve {
namespace {

glm::vec3 terrainColor(float height) {
    if (height < 0.34F) {
        return {0.08F, 0.25F + height * 0.45F, 0.65F};
    }
    if (height < 0.52F) {
        return {0.18F, 0.55F, 0.24F};
    }
    if (height < 0.72F) {
        return {0.42F, 0.35F, 0.20F};
    }
    return {0.86F, 0.88F, 0.82F};
}

} // namespace

TerrainApp::TerrainApp() {
    std::random_device rd;
    std::uint32_t seed = rd();
    generators.push_back(std::make_unique<wgen::ValueNoiseGenerator>(wgen::ValueNoiseGenerator(seed)));
    generators.push_back(std::make_unique<wgen::LayeredSinNoiseGenerator>(wgen::LayeredSinNoiseGenerator(seed)));

    loadTerrain();
}

void TerrainApp::loadTerrain() {
    constexpr std::size_t width = 96;
    constexpr std::size_t height = 64;
    // const auto heightMap =  wgen::generatePreview(width, height, 7);
    const auto heightMap = generators[used_generator]->generateheightMap(width, height);

    std::vector<Vertex2d> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve((width - 1) * (height - 1) * 4);
    indices.reserve((width - 1) * (height - 1) * 6);

    for (std::size_t y = 0; y + 1 < height; ++y) {
        for (std::size_t x = 0; x + 1 < width; ++x) {
            const float left = -1.5F + 3.0F * static_cast<float>(x) / static_cast<float>(width - 1);
            const float right = -1.5F + 3.0F * static_cast<float>(x + 1) / static_cast<float>(width - 1);
            const float top = -1.0F + 2.0F * static_cast<float>(y) / static_cast<float>(height - 1);
            const float bottom = -1.0F + 2.0F * static_cast<float>(y + 1) / static_cast<float>(height - 1);
            const float sample = heightMap.at(x, y);
            const glm::vec3 color = terrainColor(sample);
            const auto base = static_cast<std::uint32_t>(vertices.size());

            vertices.insert(
                vertices.end(),
                {{{left, top}, color}, {{right, top}, color}, {{right, bottom}, color}, {{left, bottom}, color}});
            indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
    }

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
