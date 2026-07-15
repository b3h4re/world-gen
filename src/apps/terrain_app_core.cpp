#include "terrain_app_core.hpp"

#include "terrain/generators/2d/generator_compute_capabilities.hpp"
#include "terrain/generators/2d/generator_factory.hpp"
#include "terrain/utils/hash_random.hpp"
#include "terrain/terrain.hpp"
#include "terrain/planet/planet_patch_mesh.hpp"
#include "terrain/planet/terrain_field.hpp"

#include <algorithm>
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

void TerrainAppCore::regenerateTerrain(wgen::SeedType seed, TerrainGenerationTarget target) {
    if (terrainJobRunning_) {
        return;
    }

    config_.terrainConfig.seed = seed;
    const auto terrainConfig = config_.terrainConfig;
    const auto planetConfig = config_.planetConfig;
    const auto planetPipelineSpec = planetPipelineSpec_;
    const auto previousHeightMap = activeHeightMap_;
    const auto previousTerrainField = activeTerrainField_;
    const std::uint8_t planetPatchLevel = fixedPlanetPatchLevel_;
    const bool buildPlane = target == TerrainGenerationTarget::Terrain ||
        target == TerrainGenerationTarget::All;
    const bool buildPlanet = target == TerrainGenerationTarget::Planet ||
        target == TerrainGenerationTarget::All;
    const std::uint64_t terrainEpoch = buildPlanet
        ? ++nextTerrainEpoch_
        : activeTerrainEpoch_;
    const std::uint64_t requestRevision = buildPlanet
        ? ++desiredPlanetRequestRevision_
        : desiredPlanetRequestRevision_;
    const std::vector<wgen::PlanetPatchId> publishedIds = publishedPlanetPatchIds();

    terrainJobRunning_ = true;
    terrainGenerationJob_ = std::async(std::launch::async, [
        this,
        terrainConfig,
        planetConfig,
        planetPipelineSpec,
        previousHeightMap,
        previousTerrainField,
        target,
        planetPatchLevel,
        buildPlane,
        buildPlanet,
        terrainEpoch,
        requestRevision,
        publishedIds
    ] {
        wgen::HeightMap<float> heightMap;
        wgen::TerrainFieldSnapshot terrainField;

        if (target == TerrainGenerationTarget::Terrain ||
                target == TerrainGenerationTarget::All ||
                previousHeightMap.width() < 2 || previousHeightMap.height() < 2) {
            std::lock_guard lock{generatorsMutex_};
            setGeneratorSeeds(generators_, terrainConfig.seed);
            heightMap = generateHeightMap(terrainConfig).normal();
        } else {
            heightMap = previousHeightMap;
        }

        if (buildPlanet) {
            terrainField = wgen::buildTerrainFieldSnapshot(
                planetPipelineSpec,
                terrainConfig.seed,
                planetConfig.radius);
        } else {
            terrainField = previousTerrainField;
        }

        TerrainJobResult result;
        result.heightMap = std::move(heightMap);
        result.terrainField = std::move(terrainField);
        result.terrainEpoch = terrainEpoch;
        if (buildPlane) {
            result.terrainMesh = buildPlaneMeshData(result.heightMap);
        }
        if (buildPlanet) {
            result.planetBatch = buildPlanetPatchBatch(
                result.terrainField,
                planetPatchLevel,
                PlanetPatchVersion{
                    .terrainEpoch = terrainEpoch,
                    .dependencyRevision = 0,
                    .requestRevision = requestRevision,
                },
                publishedIds);
        }

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
    ++desiredPlanetRequestRevision_;
    if (terrainJobRunning_) {
        planetRemeshPending_ = true;
        return;
    }

    if (activeTerrainField_ != nullptr) {
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
        activeTerrainField_ = result.terrainField;
        activeTerrainEpoch_ = result.terrainEpoch;
        terrainJobRunning_ = false;
        if (!discardStalePlanetPatchMeshBatch(
                result.planetBatch,
                desiredPlanetRequestRevision_) && result.planetBatch) {
            publishPlanetPatchBatch(*result.planetBatch);
        }
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
            .firstVisibleLod = static_cast<std::uint8_t>(std::min<std::size_t>(
                n,
                wgen::MAX_PLANET_PATCH_LEVEL)),
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

TerrainPlaneMeshData TerrainAppCore::buildPlaneMeshData(
        const wgen::HeightMap<float>& heightMap) {
    TerrainPlaneMeshData data;
    appendHeightMapMesh(heightMap, -1.0F, 1.0F, -1.0F, 1.0F, data.vertices2d, data.indices2d);
    appendHeightMapMesh3d(heightMap, data.vertices3d, data.indices3d);
    return data;
}

PlanetPatchMeshBatch TerrainAppCore::buildPlanetPatchBatch(
        const wgen::TerrainFieldSnapshot& terrainField,
        std::uint8_t planetPatchLevel,
        const PlanetPatchVersion& version,
        const std::vector<wgen::PlanetPatchId>& publishedIds) {
    if (terrainField == nullptr) {
        throw std::invalid_argument("planet patch batch requires a terrain field snapshot");
    }

    return makePlanetPatchMeshBatch(
        buildFixedLevelPlanetPatchMeshes(
            terrainField->radius(),
            planetPatchLevel,
            version,
            [&terrainField, planetPatchLevel](const wgen::PlanetSurfaceSample& surface) {
                return terrainField->sample(surface, planetPatchLevel);
            }),
        publishedIds,
        version.terrainEpoch,
        version.requestRevision);
}

std::vector<wgen::PlanetPatchId> TerrainAppCore::publishedPlanetPatchIds() const {
    std::vector<wgen::PlanetPatchId> result{
        publishedPlanetPatchIds_.begin(),
        publishedPlanetPatchIds_.end(),
    };
    std::sort(result.begin(), result.end(), wgen::PlanetPatchIdLess{});
    return result;
}

void TerrainAppCore::publishPlanetPatchBatch(const PlanetPatchMeshBatch& batch) {
    for (const PlanetPatchRemoval& removal : batch.removals) {
        publishedPlanetPatchIds_.erase(removal.id);
    }
    for (const PlanetPatchMeshData& upsert : batch.upserts) {
        publishedPlanetPatchIds_.insert(upsert.id);
    }
}

void TerrainAppCore::startPlanetRemeshJob() {
    const std::uint8_t planetPatchLevel = fixedPlanetPatchLevel_;
    wgen::HeightMap<float> heightMap = activeHeightMap_;
    wgen::TerrainFieldSnapshot terrainField = activeTerrainField_;
    const std::uint64_t terrainEpoch = activeTerrainEpoch_;
    const std::uint64_t requestRevision = desiredPlanetRequestRevision_;
    const std::vector<wgen::PlanetPatchId> publishedIds = publishedPlanetPatchIds();

    planetRemeshPending_ = false;
    terrainJobRunning_ = true;
    terrainGenerationJob_ = std::async(
        std::launch::async,
        [
            heightMap = std::move(heightMap),
            terrainField = std::move(terrainField),
            planetPatchLevel,
            terrainEpoch,
            requestRevision,
            publishedIds
        ]() mutable {
            TerrainJobResult result;
            result.heightMap = std::move(heightMap);
            result.terrainField = std::move(terrainField);
            result.terrainEpoch = terrainEpoch;
            result.planetBatch = buildPlanetPatchBatch(
                result.terrainField,
                planetPatchLevel,
                PlanetPatchVersion{
                    .terrainEpoch = terrainEpoch,
                    .dependencyRevision = 0,
                    .requestRevision = requestRevision,
                },
                publishedIds);
            return result;
        });
}

std::function<glm::vec3(float)> TerrainAppCore::getActiveColorFunc() const {
    return ColorMapper::getColorFunction(activeColorFuncId_);
}

} // namespace lve
