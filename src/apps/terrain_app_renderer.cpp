#include "terrain_app_renderer.hpp"

#include "renderer/objects/mesh_2d.hpp"
#include "renderer/objects/mesh_3d.hpp"

#include <utility>
#include <vulkan/vulkan.h>

namespace lve {

TerrainAppRenderer::TerrainAppRenderer() : TerrainAppRenderer(wgen::WindowConfig{}) {}

TerrainAppRenderer::TerrainAppRenderer(const wgen::WindowConfig& config)
    : window_{config.width, config.height, "World Generator"}, device_{window_},
    renderer_{window_, device_, config.present_mode}, colorMapper_{device_} {
    initDescriptorPool();
    window_.setSurfaceAboutToBeDestroyedCallback([this] {
        shutdownVulkanResources();
    });
}

TerrainAppRenderer::~TerrainAppRenderer() {
    window_.setSurfaceAboutToBeDestroyedCallback({});
    shutdownVulkanResources();
}

void TerrainAppRenderer::setDesiredPresentMode(PresentMode desiredPresentMode) {
    desiredPresentMode_ = desiredPresentMode;
    renderer_.setDesiredPresentMode(desiredPresentMode);
}

void TerrainAppRenderer::setTerrainMesh(TerrainMeshData data) {
    vkDeviceWaitIdle(device_.device());
    objects2d_ = makeObjects2d(data);
    objects3d_ = makeObjects3d(data);
    objectsPlanet_ = makeObjectsPlanet(data);
}

void TerrainAppRenderer::applyTerrainMesh(int frameIndex, TerrainMeshData data) {
    retiredObjects_[frameIndex].objects2d = std::move(objects2d_);
    retiredObjects_[frameIndex].objects3d = std::move(objects3d_);
    retiredObjects_[frameIndex].objectsPlanet = std::move(objectsPlanet_);
    objects2d_ = makeObjects2d(data);
    objects3d_ = makeObjects3d(data);
    objectsPlanet_ = makeObjectsPlanet(data);
}

void TerrainAppRenderer::clearRetiredObjects(int frameIndex) {
    retiredObjects_[frameIndex].clear();
}

void TerrainAppRenderer::waitIdle() {
    vkDeviceWaitIdle(device_.device());
}

void TerrainAppRenderer::shutdownVulkanResources() {
    if (vulkanResourcesShutdown_) {
        return;
    }

    vulkanResourcesShutdown_ = true;
    if (renderer_.isFrameInProgress()) {
        renderer_.abortFrame();
    }

    waitIdle();
    retiredObjects_ = {};
    objects3d_.clear();
    objects2d_.clear();
    objectsPlanet_.clear();
    globalPool_.reset();
    renderer_.destroySwapChain();
}

void TerrainAppRenderer::initDescriptorPool() {
    globalPool_ = LveDescriptorPool::Builder(device_)
        .setMaxSets(100)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LveSwapChain::MAX_FRAMES_IN_FLIGHT + 1)
        .build();
}

std::vector<GameObject2d> TerrainAppRenderer::makeObjects2d(const TerrainMeshData& data) {
    std::vector<GameObject2d> objects;
    auto mesh = std::make_shared<Mesh2d>(device_, data.vertices2d, data.indices2d);
    objects.push_back({std::move(mesh), {}});
    return objects;
}

std::vector<GameObject3d> TerrainAppRenderer::makeObjects3d(const TerrainMeshData& data) {
    std::vector<GameObject3d> objects;
    auto mesh = std::make_shared<Mesh3d>(device_, data.vertices3d, data.indices3d);
    objects.push_back({std::move(mesh), {}});
    return objects;
}

std::vector<GameObject3d> TerrainAppRenderer::makeObjectsPlanet(const TerrainMeshData& data) {
    std::vector<GameObject3d> objects;
    auto mesh = std::make_shared<Mesh3d>(device_, data.verticesPlanet, data.indicesPlanet);
    objects.push_back({std::move(mesh), {}});
    return objects;
}

} // namespace lve
