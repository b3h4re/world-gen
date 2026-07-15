#include "terrain_app_core.hpp"

#include "terrain/generators/2d/generator_compute_capabilities.hpp"
#include "terrain/generators/2d/generator_factory.hpp"
#include "terrain/generators/3d/generator_factory.hpp"
#include "terrain/utils/hash_random.hpp"
#include "terrain/terrain.hpp"
#include "terrain/planet/planet_patch_mesh.hpp"

#include <cstdint>
#include <vector>
#include <chrono>
#include <cmath>
#include <future>
#include <glm/glm.hpp>
#include <memory>
#include <random>
#include <stdexcept>
#include <utility>

namespace lve {

void appendHeightMapMesh(
        const wgen::HeightMap<float>& heightMap,
        float minX,
        float maxX,
        float minY,
        float maxY,
        std::vector<Vertex2d>& vertices,
        std::vector<std::uint32_t>& indices) {
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
            const auto base = static_cast<std::uint32_t>(vertices.size());

            vertices.insert(
                vertices.end(),
                {{{left, top}, sample}, {{right, top}, sample}, {{right, bottom}, sample}, {{left, bottom}, sample}});
            indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
    }
}

void appendHeightMapMesh3d(
        const wgen::HeightMap<float>& heightMap,
        std::vector<Vertex3d>& vertices,
        std::vector<std::uint32_t>& indices) {
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
                sample
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

TerrainAppCore::TerrainAppCore() : TerrainAppCore(wgen::AppConfig{}) {}

TerrainAppCore::TerrainAppCore(const wgen::AppConfig& config)
    : config_{config},
      pipelineSpec_{defaultPipelineSpec(config_.terrainConfig)},
      planetPipelineSpec_{defaultPlanetPipelineSpec(config_.planetConfig)} {
    const wgen::SeedType seed = activeSeed();
    initGenerators(generators_, pipelineSpec_, seed);
}

TerrainMeshData TerrainAppCore::loadTerrain() {
    activeHeightMap_ = generateHeightMap(config_.terrainConfig).normal();
    activeCubeSphere_ = generateCubeSphere(config_.terrainConfig, config_.planetConfig);
    activeCubeSphere_.normalize();
    return buildMeshData(activeHeightMap_, activeCubeSphere_, fixedPlanetPatchLevel_);
}

void TerrainAppCore::regenerateTerrain(wgen::SeedType seed, TerrainGenerationTarget target) {
    if (terrainJobRunning_) {
        return;
    }

    config_.terrainConfig.seed = seed;
    const auto terrainConfig = config_.terrainConfig;
    const auto planetConfig = config_.planetConfig;
    const auto colorFunc = getActiveColorFunc();
    const std::uint8_t planetPatchLevel = fixedPlanetPatchLevel_;

    terrainJobRunning_ = true;
    terrainGenerationJob_ = std::async(std::launch::async, [this, terrainConfig, planetConfig, colorFunc, target, planetPatchLevel] {
        wgen::HeightMap<float> heightMap;
        wgen::CubeSphere<float> cubeSphere;

        {
            std::lock_guard lock{generatorsMutex_};

            if (target == TerrainGenerationTarget::Terrain || target == TerrainGenerationTarget::All) {
                setGeneratorSeeds(generators_, terrainConfig.seed);
                heightMap = generateHeightMap(terrainConfig).normal();
            } else {
                heightMap = activeHeightMap_;
            }

            if (target == TerrainGenerationTarget::Planet || target == TerrainGenerationTarget::All) {
                cubeSphere = generateCubeSphere(terrainConfig, planetConfig);
                cubeSphere.normalize();
            } else {
                cubeSphere = activeCubeSphere_;
            }
        }

        TerrainJobResult result;
        result.heightMap = std::move(heightMap);
        result.cubeSphere = std::move(cubeSphere);

        appendHeightMapMesh(
            result.heightMap,
            -1.0F,
            1.0F,
            -1.0F,
            1.0F,
            result.data.vertices2d,
            result.data.indices2d
        );

        appendHeightMapMesh3d(
            result.heightMap,
            result.data.vertices3d,
            result.data.indices3d
        );

        result.data.planetPatches = buildFixedLevelPlanetPatchMeshes(result.cubeSphere, planetPatchLevel);

        return result;
    });
}

void TerrainAppCore::rotateColorFunction() {
    // -> TerrainColorStandard -> BlackAndWhite ->
    switch (activeColorFuncId_) {
        case ColorFunctions::TerrainColorStandard:
            activeColorFuncId_ = ColorFunctions::BlackAndWhite;
            return;
        case ColorFunctions::BlackAndWhite:
            activeColorFuncId_ = ColorFunctions::TerrainColorStandard;
            return;
    }
}

void TerrainAppCore::setPipeline(wgen::GeneratorPipelineSpec pipeline) {
    if (terrainJobRunning_) {
        return;
    }

    std::lock_guard lock{generatorsMutex_};
    const wgen::SeedType seed = activeSeed();
    pipelineSpec_ = std::move(pipeline);
    initGenerators(generators_, pipelineSpec_, seed);
    usedGenerator_ = 0;
}

void TerrainAppCore::setPlanetPipeline(wgen::Generator3dPipelineSpec pipeline) {
    if (terrainJobRunning_) {
        return;
    }

    std::lock_guard lock{generatorsMutex_};
    planetPipelineSpec_ = std::move(pipeline);
}

void TerrainAppCore::setPlanetShape(std::size_t resolution, float radius) {
    if (resolution == 1) {
        throw std::invalid_argument("cube sphere resolution must be 0 (automatic) or at least 2");
    }
    if (!std::isfinite(radius) || radius <= 0.0F) {
        throw std::invalid_argument("cube sphere radius must be finite and positive");
    }

    config_.planetConfig.resolution = resolution;
    config_.planetConfig.radius = radius;
}

void TerrainAppCore::requestFixedPlanetPatchLevel(std::uint8_t level) {
    if (level > MAX_FIXED_PLANET_PATCH_LEVEL) {
        throw std::invalid_argument("fixed planet patch level exceeds the supported maximum");
    }
    if (level == fixedPlanetPatchLevel_) {
        return;
    }

    fixedPlanetPatchLevel_ = level;
    if (terrainJobRunning_) {
        planetRemeshPending_ = true;
        return;
    }

    if (activeCubeSphere_.resolution() >= 2) {
        startPlanetRemeshJob();
    }
}

wgen::GeneratorPipelineSpec TerrainAppCore::currentPipeline() const {
    return pipelineSpec_;
}

wgen::Generator3dPipelineSpec TerrainAppCore::currentPlanetPipeline() const {
    return planetPipelineSpec_;
}

std::optional<TerrainJobResult> TerrainAppCore::tryTakeFinishedTerrainJob() {
    if (!terrainJobRunning_) {
        return std::nullopt;
    }

    if (terrainGenerationJob_.valid() && terrainGenerationJob_.wait_for(std::chrono::seconds{0}) == std::future_status::ready) {
        TerrainJobResult result = terrainGenerationJob_.get();
        activeHeightMap_ = result.heightMap;
        activeCubeSphere_ = result.cubeSphere;
        terrainJobRunning_ = false;
        if (planetRemeshPending_) {
            startPlanetRemeshJob();
        }
        return result;
    }

    return std::nullopt;
}

void TerrainAppCore::initGenerators(
        std::vector<std::unique_ptr<wgen::Generator>>& generators,
        const wgen::GeneratorPipelineSpec& pipelineSpec,
        wgen::SeedType seed) {
    generators.clear();
    generators.reserve(pipelineSpec.size());
    for (const wgen::GeneratorSpec& spec : pipelineSpec) {
        generators.push_back(wgen::makePipelineGenerator(spec, seed));
    }
    setGeneratorSeeds(generators, seed);
}

void TerrainAppCore::setGeneratorSeeds(
        std::vector<std::unique_ptr<wgen::Generator>>& generators,
        wgen::SeedType seed) {
    const std::vector<wgen::SeedType> seeds = generatorSeeds(generators.size(), seed);
    for (std::size_t i = 0; i < generators.size(); ++i) {
        generators[i]->setSeed(seeds[i]);
    }
}

std::vector<wgen::SeedType> TerrainAppCore::generatorSeeds(std::size_t count, wgen::SeedType seed) {
    std::vector<wgen::SeedType> seeds;
    seeds.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        seeds.push_back(seed);
        seed = wgen::hashSeed(seed);
    }

    return seeds;
}

wgen::GeneratorPipelineSpec TerrainAppCore::defaultPipelineSpec(const wgen::TerrainConfig& terrainConfig) {
    wgen::GeneratorPipelineSpec spec{};
    const float lacunarity = 2.5F;
    const float persistance = 0.5F;
    for (std::size_t n = 0; n < 5; ++n) {
        wgen::GeneratorSpec genSpec{
            .kind = wgen::GeneratorKind::PerlinNoise,
            .config = wgen::PerlinNoiseGeneratorSpec{
                .dotsPerCell = terrainConfig.perlin.dotsPerCell,
            },
            .octaveSettings = wgen::GeneratorOctaveSettings{
                .numOctave = n,
                .lacunarity = lacunarity,
                .persistance = persistance
            }
        };
        spec.push_back(genSpec);
    }
    return spec;
}

wgen::Generator3dPipelineSpec TerrainAppCore::defaultPlanetPipelineSpec(const wgen::PlanetConfig& planetConfig) {
    wgen::Generator3dPipelineSpec spec{};
    for (std::size_t n = 0; n < planetConfig.octaves; ++n) {
        spec.push_back(wgen::Generator3dSpec{
            .kind = wgen::Generator3dKind::PerlinNoise,
            .config = wgen::PerlinNoise3dGeneratorSpec{
                .cellSize = planetConfig.perlinCellSize,
            },
            .scale = 1.0F,
            .computeMethod = planetConfig.computeMethod,
            .octaveSettings = wgen::GeneratorOctaveSettings{
                .numOctave = n,
                .lacunarity = planetConfig.lacunarity,
                .persistance = planetConfig.persistence,
            },
        });
    }

    return spec;
}

wgen::SeedType TerrainAppCore::activeSeed() const {
    if (!generators_.empty() && usedGenerator_ < generators_.size()) {
        return generators_[usedGenerator_]->getSeed();
    }

    if (config_.terrainConfig.setSeed) {
        return config_.terrainConfig.seed;
    }

    std::random_device rd;
    return rd();
}

wgen::HeightMap<float> TerrainAppCore::generateHeightMap(const wgen::TerrainConfig& terrainConfig) {
    wgen::HeightMap<float> result{terrainConfig.width, terrainConfig.height};

    std::vector<std::future<wgen::HeightMap<float>>> cpuFutures;
    std::vector<GpuGeneratorRequest> gpuRequests;
    cpuFutures.reserve(pipelineSpec_.size());
    gpuRequests.reserve(pipelineSpec_.size());

    for (std::size_t i = 0; i < pipelineSpec_.size(); ++i) {
        const wgen::GeneratorSpec& spec = pipelineSpec_[i];
        if (spec.computeMethod == wgen::TerrainComputeMethod::VulkanCompute &&
                wgen::generatorSupportsComputeMethod(spec.kind, wgen::TerrainComputeMethod::VulkanCompute)) {
            gpuRequests.push_back({
                .spec = spec,
                .seed = generators_[i]->getSeed(),
            });
            continue;
        }

        cpuFutures.push_back(threadPool_.submit([this, i, terrainConfig] {
            return wgen::map(
                generateHeightMapCpu(i, terrainConfig),
                wgen::multiplyFunction(pipelineSpec_[i].scale * wgen::generatorOctaveAmplitude(pipelineSpec_[i]))
            );
        }));
    }

    std::future<wgen::HeightMap<float>> gpuFuture;
    if (!gpuRequests.empty()) {
        gpuFuture = threadPool_.submit([this, gpuRequests = std::move(gpuRequests), terrainConfig] {
            return generateHeightMapGpu(gpuRequests, terrainConfig);
        });
    }

    for (std::future<wgen::HeightMap<float>>& future : cpuFutures) {
        result += future.get();
    }
    if (gpuFuture.valid()) {
        result += gpuFuture.get();
    }

    return result;
}

wgen::HeightMap<float> TerrainAppCore::generateHeightMapCpu(
        std::size_t generatorIndex,
        const wgen::TerrainConfig& terrainConfig) {
    return generators_[generatorIndex]->generateHeightMap(terrainConfig.width, terrainConfig.height);
}

wgen::HeightMap<float> TerrainAppCore::generateHeightMapGpu(
        const std::vector<GpuGeneratorRequest>& requests,
        const wgen::TerrainConfig& terrainConfig) {
    if (gpuPipeline_ == nullptr) {
        gpuPipeline_ = std::make_unique<GpuTerrainPipeline>();
    }

    return gpuPipeline_->generateHeightMap(requests, terrainConfig.width, terrainConfig.height);
}

wgen::CubeSphere<float> TerrainAppCore::generateCubeSphere(
        const wgen::TerrainConfig& terrainConfig,
        const wgen::PlanetConfig& planetConfig) {
    const std::size_t resolution = planetConfig.resolution == 0
        ? std::min(terrainConfig.width, terrainConfig.height)
        : planetConfig.resolution;
    wgen::CubeSphere<float> result{planetConfig.radius, resolution, 0.0F};

    std::vector<GpuPlanetGeneratorRequest> gpuRequests;
    gpuRequests.reserve(planetPipelineSpec_.size());
    const std::vector<wgen::SeedType> seeds = generatorSeeds(planetPipelineSpec_.size(), terrainConfig.seed);

    for (std::size_t i = 0; i < planetPipelineSpec_.size(); ++i) {
        const wgen::Generator3dSpec& spec = planetPipelineSpec_[i];
        if (spec.computeMethod == wgen::TerrainComputeMethod::VulkanCompute &&
                wgen::generator3dSupportsVulkanCompute(spec.kind)) {
            gpuRequests.push_back({
                .spec = spec,
                .seed = seeds[i],
            });
            continue;
        }

        std::unique_ptr<wgen::Generator3d> generator = wgen::makePipelineGenerator3d(spec, seeds[i]);
        result += wgen::map(
            generator->generateCubeSphere(resolution),
            wgen::multiplyFunction(spec.scale * wgen::generator3dOctaveAmplitude(spec))
        );
    }

    if (!gpuRequests.empty()) {
        result += generateCubeSphereGpu(gpuRequests, resolution, planetConfig.radius);
    }

    return result;
}

wgen::CubeSphere<float> TerrainAppCore::generateCubeSphereGpu(
        const std::vector<GpuPlanetGeneratorRequest>& requests,
        std::size_t resolution,
        float radius) {
    if (gpuPlanetPipeline_ == nullptr) {
        gpuPlanetPipeline_ = std::make_unique<GpuPlanetPipeline>();
    }

    wgen::CubeSphere<float> cubeSphere = gpuPlanetPipeline_->generateCubeSphere(requests, resolution);
    cubeSphere.setRadius(radius);
    return cubeSphere;
}

TerrainMeshData TerrainAppCore::buildMeshData(
        const wgen::HeightMap<float>& heightMap,
        const wgen::CubeSphere<float>& cubeSphere,
        std::uint8_t planetPatchLevel) {
    TerrainMeshData data;
    appendHeightMapMesh(heightMap, -1.0F, 1.0F, -1.0F, 1.0F, data.vertices2d, data.indices2d);
    appendHeightMapMesh3d(heightMap, data.vertices3d, data.indices3d);
    data.planetPatches = buildFixedLevelPlanetPatchMeshes(cubeSphere, planetPatchLevel);
    return data;
}

void TerrainAppCore::startPlanetRemeshJob() {
    const std::uint8_t planetPatchLevel = fixedPlanetPatchLevel_;
    wgen::HeightMap<float> heightMap = activeHeightMap_;
    wgen::CubeSphere<float> cubeSphere = activeCubeSphere_;

    planetRemeshPending_ = false;
    terrainJobRunning_ = true;
    terrainGenerationJob_ = std::async(
        std::launch::async,
        [heightMap = std::move(heightMap), cubeSphere = std::move(cubeSphere), planetPatchLevel]() mutable {
            TerrainJobResult result;
            result.heightMap = std::move(heightMap);
            result.cubeSphere = std::move(cubeSphere);
            result.data = buildMeshData(result.heightMap, result.cubeSphere, planetPatchLevel);
            return result;
        });
}

std::function<glm::vec3(float)> TerrainAppCore::getActiveColorFunc() const {
    return ColorMapper::getColorFunction(activeColorFuncId_);
}

} // namespace lve
