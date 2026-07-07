#include "terrain_app_core.hpp"

#include "terrain/generators/2d/generator_compute_capabilities.hpp"
#include "terrain/generators/2d/generator_factory.hpp"
#include "terrain/generators/3d/generator_factory.hpp"
#include "terrain/utils/hash_random.hpp"
#include "terrain/terrain.hpp"

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

void appendPlanetmesh(
        const wgen::Planet<float>& planet,
        std::vector<Vertex3d>& vertices,
        std::vector<std::uint32_t>& indices) {
    const std::size_t dots = planet.width();

    vertices.reserve(vertices.size() + dots * dots);
    indices.reserve(indices.size() + (dots - 1) * (dots - 1) * 6);

    const float R = planet.radius();

    for (std::size_t y = 0; y < dots; ++y) {
        for (std::size_t x = 0; x < dots; ++x) {
            const float height = planet.at(x, y);

            const glm::vec3 pointDir = planet.pointUnitDir(x, y);
            glm::vec3 pos = pointDir * (height + R) / R;
            vertices.push_back({
                .position = pos,
                .height = height
            });
        }
    }

    for (std::size_t y = 0; y + 1 < dots; ++y) {
        for (std::size_t x = 0; x + 1 < dots; ++x) {
            const auto topLeft = static_cast<std::uint32_t>(y * dots + x);
            const auto topRight = static_cast<std::uint32_t>(y * dots + x + 1);
            const auto bottomLeft = static_cast<std::uint32_t>((y + 1) * dots + x);
            const auto bottomRight = static_cast<std::uint32_t>((y + 1) * dots + x + 1);

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
    activePlanet_ = generatePlanet(config_.terrainConfig);
    activePlanet_.normalize();
    return buildMeshData(activeHeightMap_, activePlanet_);
}

void TerrainAppCore::regenerateTerrain(wgen::SeedType seed, TerrainGenerationTarget target) {
    if (terrainJobRunning_) {
        return;
    }

    config_.terrainConfig.seed = seed;
    const auto terrainConfig = config_.terrainConfig;
    const auto colorFunc = getActiveColorFunc();

    terrainJobRunning_ = true;
    terrainGenerationJob_ = std::async(std::launch::async, [this, terrainConfig, colorFunc, target] {
        wgen::HeightMap<float> heightMap;
        wgen::Planet<float> planet;

        {
            std::lock_guard lock{generatorsMutex_};

            if (target == TerrainGenerationTarget::Terrain || target == TerrainGenerationTarget::All) {
                setGeneratorSeeds(generators_, terrainConfig.seed);
                heightMap = generateHeightMap(terrainConfig).normal();
            } else {
                heightMap = activeHeightMap_;
            }

            if (target == TerrainGenerationTarget::Planet || target == TerrainGenerationTarget::All) {
                planet = generatePlanet(terrainConfig);
                planet.normalize();
            } else {
                planet = activePlanet_;
            }
        }

        TerrainJobResult result;
        result.heightMap = std::move(heightMap);
        result.planet = std::move(planet);

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

        appendPlanetmesh(
            result.planet,
            result.data.verticesPlanet,
            result.data.indicesPlanet
        );

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
        activePlanet_ = result.planet;
        terrainJobRunning_ = false;
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

wgen::Planet<float> TerrainAppCore::generatePlanet(const wgen::TerrainConfig& terrainConfig) {
    const std::size_t dots = config_.planetConfig.resolution == 0
        ? std::min(terrainConfig.width, terrainConfig.height)
        : config_.planetConfig.resolution;
    wgen::Planet<float> result{dots, 0.0F};
    result.setRadius(config_.planetConfig.radius);
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
            generator->generatePlanet(dots),
            wgen::multiplyFunction(spec.scale * wgen::generator3dOctaveAmplitude(spec))
        );
    }

    if (!gpuRequests.empty()) {
        result += generatePlanetGpu(gpuRequests, terrainConfig);
        result.setRadius(config_.planetConfig.radius);
    }

    return result;
}

wgen::Planet<float> TerrainAppCore::generatePlanetGpu(
        const std::vector<GpuPlanetGeneratorRequest>& requests,
        const wgen::TerrainConfig& terrainConfig) {
    if (gpuPlanetPipeline_ == nullptr) {
        gpuPlanetPipeline_ = std::make_unique<GpuPlanetPipeline>();
    }

    const std::size_t dots = config_.planetConfig.resolution == 0
        ? std::min(terrainConfig.width, terrainConfig.height)
        : config_.planetConfig.resolution;
    wgen::Planet<float> planet = gpuPlanetPipeline_->generatePlanet(requests, dots);
    planet.setRadius(config_.planetConfig.radius);
    return planet;
}

TerrainMeshData TerrainAppCore::buildMeshData(const wgen::HeightMap<float>& heightMap, const wgen::Planet<float>& planet) {
    TerrainMeshData data;
    appendHeightMapMesh(heightMap, -1.0F, 1.0F, -1.0F, 1.0F, data.vertices2d, data.indices2d);
    appendHeightMapMesh3d(heightMap, data.vertices3d, data.indices3d);
    appendPlanetmesh(planet, data.verticesPlanet, data.indicesPlanet);
    return data;
}

std::function<glm::vec3(float)> TerrainAppCore::getActiveColorFunc() const {
    return ColorMapper::getColorFunction(activeColorFuncId_);
}

} // namespace lve
