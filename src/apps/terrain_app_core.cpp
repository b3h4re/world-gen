#include "terrain_app_core.hpp"

#include "terrain/generators/generators.hpp"
#include "terrain/terrain.hpp"

#include <chrono>
#include <glm/glm.hpp>
#include <memory>
#include <random>

namespace lve {

void appendHeightMapMesh(
        const wgen::HeightMap<float>& heightMap,
        float minX,
        float maxX,
        float minY,
        float maxY,
        std::vector<Vertex2d>& vertices,
        std::vector<std::uint32_t>& indices,
        wgen::colorFromHeightFunc colorFunc) {
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
        wgen::colorFromHeightFunc colorFunc) {
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

TerrainAppCore::TerrainAppCore() : TerrainAppCore(wgen::AppConfig{}) {}

TerrainAppCore::TerrainAppCore(const wgen::AppConfig& config) : config_{config} {
    initGenerators(generators_, config_.terrainConfig);
}

TerrainMeshData TerrainAppCore::loadTerrain() {
    const std::size_t width = config_.terrainConfig.width;
    const std::size_t height = config_.terrainConfig.height;
    activeHeightMap_ = generators_[usedGenerator_]->generateHeightMap(width, height).normal();
    return buildMeshData(activeHeightMap_);
}

void TerrainAppCore::regenerateTerrain(std::uint32_t seed) {
    if (terrainJobRunning_) {
        return;
    }

    config_.terrainConfig.seed = seed;
    const auto terrainConfig = config_.terrainConfig;
    const auto colorFunc = getActiveColorFunc();
    const std::size_t genToUse = usedGenerator_;

    terrainJobRunning_ = true;
    terrainGenerationJob_ = std::async(std::launch::async, [this, terrainConfig, colorFunc, genToUse] {
        wgen::HeightMap<float> heightMap;

        {
            std::lock_guard lock{generatorsMutex_};

            generators_[genToUse]->setSeed(terrainConfig.seed);
            heightMap = generators_[genToUse]
                ->generateHeightMap(terrainConfig.width, terrainConfig.height)
                .normal();
        }

        TerrainJobResult result;
        result.heightMap = std::move(heightMap);

        appendHeightMapMesh(
            result.heightMap,
            -1.0F,
            1.0F,
            -1.0F,
            1.0F,
            result.data.vertices2d,
            result.data.indices2d,
            colorFunc
        );

        appendHeightMapMesh3d(
            result.heightMap,
            result.data.vertices3d,
            result.data.indices3d,
            colorFunc
        );

        return result;
    });
}

void TerrainAppCore::rotateColorFunction() {
    if (terrainJobRunning_) {
        return;
    }

    activeColorFuncId_ = (activeColorFuncId_ + 1) % NUM_COLOR_FUNCTIONS;

    const auto colorFunc = getActiveColorFunc();
    const auto heightMap = activeHeightMap_;
    terrainJobRunning_ = true;

    terrainReappendJob_ = std::async(std::launch::async, [heightMap, colorFunc] {
        TerrainMeshData data;

        appendHeightMapMesh(
            heightMap,
            -1.0F,
            1.0F,
            -1.0F,
            1.0F,
            data.vertices2d,
            data.indices2d,
            colorFunc
        );

        appendHeightMapMesh3d(
            heightMap,
            data.vertices3d,
            data.indices3d,
            colorFunc
        );

        return data;
    });
}

std::optional<TerrainJobResult> TerrainAppCore::tryTakeFinishedTerrainJob() {
    if (!terrainJobRunning_) {
        return std::nullopt;
    }

    if (terrainReappendJob_.valid() && terrainReappendJob_.wait_for(std::chrono::seconds{0}) == std::future_status::ready) {
        TerrainJobResult result;
        result.data = terrainReappendJob_.get();
        terrainJobRunning_ = false;
        return result;
    }

    if (terrainGenerationJob_.valid() && terrainGenerationJob_.wait_for(std::chrono::seconds{0}) == std::future_status::ready) {
        TerrainJobResult result = terrainGenerationJob_.get();
        activeHeightMap_ = result.heightMap;
        terrainJobRunning_ = false;
        return result;
    }

    return std::nullopt;
}

void TerrainAppCore::initGenerators(
        std::vector<std::unique_ptr<wgen::Generator>>& generators,
        const wgen::TerrainConfig& terrainConfig) {
    generators.clear();
    std::uint32_t seed;
    if (terrainConfig.setSeed) {
        seed = terrainConfig.seed;
    } else {
        std::random_device rd;
        seed = rd();
    }

    generators.push_back(std::make_unique<wgen::OctaveGenerator<wgen::PerlinNoise2d>>(
        wgen::OctaveGenerator<wgen::PerlinNoise2d>(
            terrainConfig.perlin.gridWidth,
            terrainConfig.perlin.gridHeight,
            terrainConfig.perlin.dotsPerCell,
            seed,
            3,
            2.5F,
            0.5F
        )
    ));
    generators.push_back(std::make_unique<wgen::DLADualFilterBlur>(wgen::DLADualFilterBlur(
        terrainConfig.dla.numSteps, seed, wgen::defaultDLAHeightFunction(terrainConfig.dla.heightFuncScale),
        terrainConfig.dla.fill, terrainConfig.dla.jiggle)));
    generators.push_back(std::make_unique<wgen::WorleyNoise2d>(wgen::WorleyNoise2d(
        terrainConfig.worley.gridWidth,
        terrainConfig.worley.gridHeight,
        terrainConfig.worley.dotsPerCell,
        seed,
        terrainConfig.worley.p,
        terrainConfig.worley.numPoints
    )));
    generators.push_back(std::make_unique<wgen::WaveletNoise2d>(wgen::WaveletNoise2d(
        terrainConfig.wavelet.gridWidth,
        terrainConfig.wavelet.gridHeight,
        seed,
        wgen::defaultReconstructionKernel
    )));
    generators.push_back(std::make_unique<wgen::SimplexNoise2d>(wgen::SimplexNoise2d(
        terrainConfig.simplex.gridWidth,
        terrainConfig.simplex.gridHeight,
        terrainConfig.simplex.dotsPerCell,
        seed
    )));
    generators.push_back(std::make_unique<wgen::PerlinNoise2d>(wgen::PerlinNoise2d(
        terrainConfig.perlin.gridWidth,
        terrainConfig.perlin.gridHeight,
        terrainConfig.perlin.dotsPerCell,
        seed,
        wgen::defaultPerlinInterp
    )));
    generators.push_back(std::make_unique<wgen::ValueNoiseGenerator>(wgen::ValueNoiseGenerator(seed)));
    generators.push_back(std::make_unique<wgen::LayeredSinNoiseGenerator>(wgen::LayeredSinNoiseGenerator(seed)));
}

TerrainMeshData TerrainAppCore::buildMeshData(const wgen::HeightMap<float>& heightMap) {
    TerrainMeshData data;
    appendHeightMapMesh(heightMap, -1.0F, 1.0F, -1.0F, 1.0F, data.vertices2d, data.indices2d, getActiveColorFunc());
    appendHeightMapMesh3d(heightMap, data.vertices3d, data.indices3d, getActiveColorFunc());
    return data;
}

wgen::colorFromHeightFunc TerrainAppCore::getActiveColorFunc() const {
    return COLOR_FUNCTIONS[activeColorFuncId_];
}

} // namespace lve
