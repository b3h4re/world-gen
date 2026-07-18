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
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace lve {

namespace {

wgen::PlanetLodStreamingConfig planetStreamingConfig(
        const wgen::PlanetConfig& config) {
    if (config.lodPatchGenerationBudget < 4 ||
            config.lodPatchUploadBudget == 0 ||
            config.lodPatchUploadBudget >
                wgen::DEFAULT_PLANET_RESIDENT_PATCH_BUDGET ||
            config.lodMaximumConcurrentPatchJobs == 0 ||
            config.lodMaximumConcurrentPatchJobs >
                (wgen::DEFAULT_PLANET_RESIDENT_PATCH_BUDGET - wgen::FACES.size()) /
                    config.lodPatchGenerationBudget) {
        throw std::invalid_argument("planet LOD streaming budgets are invalid");
    }
    return {
        .residentPatchBudget = wgen::DEFAULT_PLANET_RESIDENT_PATCH_BUDGET,
        .patchGenerationBudget = config.lodPatchGenerationBudget,
        .patchUploadBudget = config.lodPatchUploadBudget,
        .maximumConcurrentPatchJobs = config.lodMaximumConcurrentPatchJobs,
    };
}

wgen::PlanetLodConfig planetLodConfig(const wgen::PlanetConfig& config) {
    const wgen::PlanetLodStreamingConfig streaming = planetStreamingConfig(config);
    wgen::PlanetLodConfig result;
    result.selectedLeafBudget = streaming.residentPatchBudget -
        streaming.patchGenerationBudget * streaming.maximumConcurrentPatchJobs;
    return result;
}

wgen::LocalClipmapConfig makeLocalClipmapConfig(double planetRadiusMeters) {
    if (!std::isfinite(planetRadiusMeters) || planetRadiusMeters <= 0.0) {
        throw std::invalid_argument{
            "local clipmap planet radius must be finite and positive"};
    }
    wgen::LocalClipmapConfig result{};
    const double halfExtentCells =
        static_cast<double>((result.samplesPerAxis - 1) / 2);
    double coarsestScale = 1.0;
    for (std::uint32_t level = 1; level < result.levelCount; ++level) {
        coarsestScale *= static_cast<double>(result.levelScale);
    }
    // Keep even the tiny default demo planet comfortably short of the
    // exponential-map antipode. Earth-sized planets retain the authored 1 m
    // finest spacing.
    const double maximumSafeFinestSpacing =
        planetRadiusMeters * 0.25 / (halfExtentCells * coarsestScale);
    result.finestCellSpacingMeters = std::min(
        result.finestCellSpacingMeters,
        maximumSafeFinestSpacing);
    wgen::validateLocalClipmapConfig(result);
    return result;
}

wgen::LocalClipmapControllerConfig makeLocalClipmapControllerConfig(
        const wgen::LocalClipmapConfig& clipmapConfig,
        double planetRadiusMeters) {
    const double handoffSpacing = wgen::localClipmapCellSpacing(
        clipmapConfig,
        clipmapConfig.levelCount - 1);
    wgen::LocalClipmapControllerConfig result{
        .activationAltitudeMeters = planetRadiusMeters * 0.015,
        .deactivationAltitudeMeters = planetRadiusMeters * 0.025,
        .activationGroundResolutionMeters = handoffSpacing,
        .deactivationGroundResolutionMeters = handoffSpacing * 1.5,
        .coolingDurationSeconds = 0.25,
    };
    wgen::validateLocalClipmapControllerConfig(result);
    return result;
}

bool sameClipmapOrigin(
        const wgen::LocalClipmapCacheOrigin& lhs,
        const wgen::LocalClipmapCacheOrigin& rhs) {
    return lhs.localFrameAnchorMeters == rhs.localFrameAnchorMeters &&
        lhs.frameRevision == rhs.frameRevision && lhs.level == rhs.level &&
        lhs.centerSample == rhs.centerSample;
}

bool sameClipmapContent(
        const wgen::LocalClipmapUpdateIdentity& lhs,
        const wgen::LocalClipmapUpdateIdentity& rhs) {
    return lhs.terrainEpoch == rhs.terrainEpoch &&
        lhs.terrainDependencyRevision == rhs.terrainDependencyRevision &&
        lhs.localFrameRevision == rhs.localFrameRevision &&
        lhs.level == rhs.level &&
        lhs.snappedGridOrigin == rhs.snappedGridOrigin;
}

std::int64_t checkedClipmapCoordinateAdd(
        std::int64_t value,
        std::uint32_t offset) {
    if (value > std::numeric_limits<std::int64_t>::max() - offset) {
        throw std::overflow_error{"local clipmap mesh coordinate overflow"};
    }
    return value + static_cast<std::int64_t>(offset);
}

} // namespace

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
      planetPipelineSpec_{defaultPlanetPipelineSpec(config_.planetConfig)},
      planetLodCoordinator_{
          planetLodConfig(config_.planetConfig),
          planetStreamingConfig(config_.planetConfig),
          {
              .durationSeconds = config_.planetConfig.lodTransitionDurationSeconds,
              .debugTimeScale = config_.planetConfig.lodTransitionTimeScale,
          }},
      localClipmapConfig_{makeLocalClipmapConfig(config_.planetConfig.radius)},
      localClipmapTopology_{wgen::buildLocalClipmapTopology(localClipmapConfig_)},
      localClipmapResidency_{localClipmapConfig_},
      localClipmapController_{makeLocalClipmapControllerConfig(
          localClipmapConfig_,
          config_.planetConfig.radius)},
      publishedLocalClipmapIdentities_(localClipmapConfig_.levelCount) {
    const wgen::SeedType seed = activeSeed();
    initGenerators(generators_, pipelineSpec_, seed);
}

void TerrainAppCore::regenerateTerrain(wgen::SeedType seed, TerrainGenerationTarget target) {
    if (terrainRegenerationRunning_) {
        return;
    }

    config_.terrainConfig.seed = seed;
    const auto terrainConfig = config_.terrainConfig;
    const auto planetConfig = config_.planetConfig;
    const auto planetPipelineSpec = planetPipelineSpec_;
    const auto previousHeightMap = activeHeightMap_;
    const auto previousTerrainField = activeTerrainField_;
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
    const std::vector<wgen::PlanetPatchId> rootIds = fixedLevelPlanetPatchIds(0);

    terrainRegenerationRunning_ = true;
    planetRegenerationPending_ = buildPlanet;
    if (buildPlanet) {
        planetLodCoordinator_.cancelPendingRequestsBefore(requestRevision);
        discardStalePlanetUploads();
    }
    terrainGenerationJob_ = std::async(std::launch::async, [
        this,
        terrainConfig,
        planetConfig,
        planetPipelineSpec,
        previousHeightMap,
        previousTerrainField,
        target,
        buildPlane,
        buildPlanet,
        terrainEpoch,
        requestRevision,
        rootIds
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
                planetConfig.radius,
                planetConfig.heightSemantics);
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
                PlanetPatchVersion{
                    .terrainEpoch = terrainEpoch,
                    .dependencyRevision = 0,
                    .requestRevision = requestRevision,
                },
                rootIds,
                {},
                planetConfig.skirtDepthMultiplier);
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
    if (terrainRegenerationRunning_) {
        return;
    }

    std::lock_guard lock{generatorsMutex_};
    const wgen::SeedType seed = activeSeed();
    pipelineSpec_ = std::move(pipeline);
    initGenerators(generators_, pipelineSpec_, seed);
    usedGenerator_ = 0;
}

void TerrainAppCore::setPlanetPipeline(wgen::Generator3dPipelineSpec pipeline) {
    if (terrainRegenerationRunning_) {
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
    localClipmapConfig_ = makeLocalClipmapConfig(radius);
    localClipmapTopology_ = wgen::buildLocalClipmapTopology(
        localClipmapConfig_);
    localClipmapResidency_ = wgen::LocalClipmapHeightResidency{
        localClipmapConfig_};
    localClipmapController_ = wgen::LocalClipmapController{
        makeLocalClipmapControllerConfig(localClipmapConfig_, radius)};
    localClipmapFrame_.reset();
    localClipmapFootprint_.reset();
    hybridPlanetDrawStates_ = planetLodCoordinator_.visibleDrawStates();
    requestedLocalClipmapOrigins_.clear();
    publishedLocalClipmapIdentities_.assign(
        localClipmapConfig_.levelCount,
        std::nullopt);
    requestedLocalClipmapTerrainEpoch_ = 0;
    localClipmapRequestRevision_ = 0;
}

void TerrainAppCore::setMaximumPlanetPatchLevel(std::uint8_t level) {
    if (level == planetLodCoordinator_.lodConfig().maximumLevel) {
        return;
    }

    planetLodCoordinator_.setMaximumLevel(level);
    planetLodSelectionDirty_ = true;
}

void TerrainAppCore::updatePlanetLod(
        const wgen::PlanetLodView& view,
        double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0) {
        throw std::invalid_argument("planet LOD frame delta must be finite and non-negative");
    }
    planetLodCoordinator_.updateVisibility(view);
    planetLodCoordinator_.advanceTransitions(deltaSeconds);
    if (activeTerrainField_ == nullptr || activeTerrainEpoch_ == 0) {
        return;
    }
    if (planetRegenerationPending_) {
        return;
    }

    planetLodSelectionElapsedSeconds_ += deltaSeconds;
    const double selectionInterval = config_.planetConfig.lodSelectionIntervalSeconds;
    const bool selectionDue = planetLodSelectionDirty_ || selectionInterval == 0.0 ||
        planetLodSelectionElapsedSeconds_ >= selectionInterval;
    if (selectionDue) {
        const bool selectionChanged = planetLodCoordinator_.updateSelection(view);
        planetLodSelectionElapsedSeconds_ = 0.0;
        if (selectionChanged || planetLodSelectionDirty_) {
            ++desiredPlanetRequestRevision_;
            planetLodCoordinator_.cancelPendingRequestsBefore(
                desiredPlanetRequestRevision_);
            discardStalePlanetUploads();
            planetLodSelectionDirty_ = false;
        }
    }

    const std::size_t outstandingLimit =
        planetLodCoordinator_.streamingConfig().patchGenerationBudget *
        planetLodCoordinator_.streamingConfig().maximumConcurrentPatchJobs;
    while (planetPatchJobs_.size() <
            planetLodCoordinator_.streamingConfig().maximumConcurrentPatchJobs &&
            queuedPlanetPatchUploadCount() < outstandingLimit) {
        wgen::PlanetLodPatchPlan plan = planetLodCoordinator_.makePatchPlan();
        if (plan.upserts.empty() && plan.removals.empty()) {
            break;
        }
        if (queuedPlanetPatchUploadCount() + plan.upserts.size() >
                outstandingLimit) {
            break;
        }
        startPlanetLodJob(std::move(plan));
    }
    rebuildHybridPlanetDrawStates();
}

void TerrainAppCore::updateLocalClipmapController(
        const wgen::LocalClipmapControllerView& view,
        const wgen::LocalPlanetFrame& frame,
        double deltaSeconds) {
    wgen::validateLocalPlanetFrame(frame);
    localClipmapFrame_ = frame;
    localClipmapController_.update(
        view,
        hasCompleteCurrentLocalClipmap(frame),
        deltaSeconds);
    rebuildLocalClipmapFootprint();
    rebuildHybridPlanetDrawStates();
}

std::vector<wgen::LocalClipmapGpuUploadBatch>
TerrainAppCore::updateLocalClipmapResidency(
        const wgen::LocalPlanetFrame& frame,
        const glm::dvec2& cameraPositionInLocalFrameMeters) {
    wgen::validateLocalPlanetFrame(frame);
    if (activeTerrainField_ == nullptr || activeTerrainEpoch_ == 0 ||
            activeTerrainField_->radius() !=
                static_cast<float>(frame.planetRadiusMeters)) {
        return {};
    }

    localClipmapFrame_ = frame;
    std::vector<wgen::LocalClipmapCacheOrigin> origins;
    origins.reserve(localClipmapConfig_.levelCount);
    for (std::uint32_t level = 0;
         level < localClipmapConfig_.levelCount;
         ++level) {
        origins.push_back(wgen::snapLocalClipmapOrigin(
            localClipmapConfig_,
            level,
            {},
            frame.revision,
            cameraPositionInLocalFrameMeters));
    }

    bool targetChanged =
        requestedLocalClipmapTerrainEpoch_ != activeTerrainEpoch_ ||
        origins.size() != requestedLocalClipmapOrigins_.size();
    if (!targetChanged) {
        for (std::size_t level = 0; level < origins.size(); ++level) {
            if (!sameClipmapOrigin(
                    origins[level],
                    requestedLocalClipmapOrigins_[level])) {
                targetChanged = true;
                break;
            }
        }
    }
    if (targetChanged) {
        ++localClipmapRequestRevision_;
        if (localClipmapRequestRevision_ == 0) {
            ++localClipmapRequestRevision_;
        }
        localClipmapResidency_.requestResidency(
            activeTerrainEpoch_,
            0,
            localClipmapRequestRevision_,
            origins);
        requestedLocalClipmapTerrainEpoch_ = activeTerrainEpoch_;
        requestedLocalClipmapOrigins_ = origins;
    }

    const std::vector<wgen::LocalClipmapHeightUpdateRequest> requests =
        localClipmapResidency_.takeGenerationRequestsForFrame();
    for (const wgen::LocalClipmapHeightUpdateRequest& request : requests) {
        const wgen::LocalClipmapHeightUpdateResult result =
            wgen::generateLocalClipmapHeightUpdate(
                request,
                frame,
                activeTerrainField_,
                localClipmapConfig_);
        if (!localClipmapResidency_.publishHeightUpdate(result)) {
            throw std::logic_error{
                "synchronous local clipmap generation became stale"};
        }
    }
    return localClipmapResidency_.takeGpuUploadBatchesForFrame();
}

bool TerrainAppCore::commitLocalClipmapGpuUpload(
        const wgen::LocalClipmapGpuUploadBatch& batch) {
    return localClipmapResidency_.commitGpuUploadBatch(batch);
}

LocalClipmapMeshUpdate TerrainAppCore::takeLocalClipmapMeshUpdate() {
    LocalClipmapMeshUpdate update;
    update.cacheStates.reserve(localClipmapConfig_.levelCount);
    if (!localClipmapFrame_) {
        for (std::uint32_t level = 0;
             level < localClipmapConfig_.levelCount;
             ++level) {
            update.cacheStates.push_back({.level = level});
        }
        return update;
    }

    const std::uint32_t samplesPerAxis =
        localClipmapConfig_.samplesPerAxis;
    for (std::uint32_t level = 0;
         level < localClipmapConfig_.levelCount;
         ++level) {
        const std::optional<wgen::LocalClipmapUpdateIdentity> activeIdentity =
            localClipmapResidency_.activeIdentity(level);
        const std::optional<wgen::LocalClipmapUpdateIdentity> desiredIdentity =
            localClipmapResidency_.desiredIdentity(level);
        const bool current = activeIdentity && desiredIdentity &&
            *activeIdentity == *desiredIdentity &&
            !localClipmapResidency_.hasPendingTarget(level);
        update.cacheStates.push_back({
            .level = level,
            .hasActiveCache = activeIdentity.has_value(),
            .activeCacheIsCurrent = current,
        });
        if (!activeIdentity) {
            publishedLocalClipmapIdentities_[level].reset();
            continue;
        }
        if (publishedLocalClipmapIdentities_[level] &&
                *publishedLocalClipmapIdentities_[level] == *activeIdentity) {
            continue;
        }
        if (publishedLocalClipmapIdentities_[level] && sameClipmapContent(
                *publishedLocalClipmapIdentities_[level],
                *activeIdentity)) {
            publishedLocalClipmapIdentities_[level] = activeIdentity;
            continue;
        }

        const std::optional<wgen::LocalClipmapCacheOrigin> origin =
            localClipmapResidency_.activeOrigin(level);
        if (!origin || origin->frameRevision != localClipmapFrame_->revision) {
            continue;
        }
        const wgen::LocalClipmapGridCoordinate minimum =
            wgen::localClipmapWindowMinimum(localClipmapConfig_, *origin);
        std::vector<float> heights;
        heights.reserve(localClipmapTopology_.vertices.size());
        for (std::uint32_t y = 0; y < samplesPerAxis; ++y) {
            for (std::uint32_t x = 0; x < samplesPerAxis; ++x) {
                const wgen::LocalClipmapGridCoordinate coordinate{
                    checkedClipmapCoordinateAdd(minimum.x, x),
                    checkedClipmapCoordinateAdd(minimum.y, y),
                };
                const std::optional<float> height =
                    localClipmapResidency_.activeGpuHeight(level, coordinate);
                if (!height) {
                    throw std::logic_error{
                        "active local clipmap cache contains an invalid sample"};
                }
                heights.push_back(*height);
            }
        }
        update.upserts.push_back(buildLocalClipmapLevelMesh(
            *localClipmapFrame_,
            localClipmapTopology_,
            *origin,
            *activeIdentity,
            heights));
        publishedLocalClipmapIdentities_[level] = activeIdentity;
    }
    return update;
}

bool TerrainAppCore::hasCompleteCurrentLocalClipmap(
        const wgen::LocalPlanetFrame& frame) const {
    if (activeTerrainField_ == nullptr || activeTerrainEpoch_ == 0 ||
            activeTerrainField_->radius() !=
                static_cast<float>(frame.planetRadiusMeters)) {
        return false;
    }
    for (std::uint32_t level = 0;
         level < localClipmapConfig_.levelCount;
         ++level) {
        const std::optional<wgen::LocalClipmapUpdateIdentity> active =
            localClipmapResidency_.activeIdentity(level);
        const std::optional<wgen::LocalClipmapUpdateIdentity> desired =
            localClipmapResidency_.desiredIdentity(level);
        if (!active || !desired || *active != *desired ||
                localClipmapResidency_.hasPendingTarget(level) ||
                active->terrainEpoch != activeTerrainEpoch_ ||
                active->localFrameRevision != frame.revision) {
            return false;
        }
    }
    return true;
}

void TerrainAppCore::rebuildLocalClipmapFootprint() {
    localClipmapFootprint_.reset();
    if (!localClipmapController_.ownsCoverage() || !localClipmapFrame_ ||
            !hasCompleteCurrentLocalClipmap(*localClipmapFrame_)) {
        return;
    }
    const std::uint32_t outerLevel = localClipmapConfig_.levelCount - 1;
    const std::optional<wgen::LocalClipmapCacheOrigin> outerOrigin =
        localClipmapResidency_.activeOrigin(outerLevel);
    if (!outerOrigin ||
            outerOrigin->frameRevision != localClipmapFrame_->revision) {
        return;
    }
    const double spacing = wgen::localClipmapCellSpacing(
        localClipmapConfig_,
        outerLevel);
    const glm::dvec2 center = wgen::localClipmapSamplePosition(
        *outerOrigin,
        outerOrigin->centerSample,
        spacing);
    const double outerHalfExtent =
        static_cast<double>(localClipmapTopology_.outerHalfExtentCells) *
        spacing;
    const double innerHalfExtent = outerHalfExtent -
        static_cast<double>(localClipmapConfig_.overlapWidthCells) * spacing;
    localClipmapFootprint_ = wgen::LocalClipmapFootprint{
        .frame = *localClipmapFrame_,
        .centerMeters = center,
        .innerHalfExtentMeters = innerHalfExtent,
        .outerHalfExtentMeters = outerHalfExtent,
        .terrainEpoch = activeTerrainEpoch_,
    };
    wgen::validateLocalClipmapFootprint(*localClipmapFootprint_);
}

void TerrainAppCore::rebuildHybridPlanetDrawStates() {
    const std::vector<wgen::PlanetPatchDrawState>& globalStates =
        planetLodCoordinator_.visibleDrawStates();
    if (!localClipmapController_.ownsCoverage() ||
            !localClipmapFootprint_ ||
            localClipmapFootprint_->terrainEpoch != activeTerrainEpoch_) {
        hybridPlanetDrawStates_ = globalStates;
        return;
    }
    hybridPlanetDrawStates_.clear();
    hybridPlanetDrawStates_.reserve(globalStates.size());
    const bool allRequiredLevelsCurrent = localClipmapFrame_ &&
        hasCompleteCurrentLocalClipmap(*localClipmapFrame_);
    for (const wgen::PlanetPatchDrawState& state : globalStates) {
        if (!wgen::localClipmapOwnsPlanetPatch(
                state.id,
                *localClipmapFootprint_,
                allRequiredLevelsCurrent,
                activeTerrainEpoch_)) {
            hybridPlanetDrawStates_.push_back(state);
        }
    }
}

void TerrainAppCore::setPlanetLodTransitionTimeScale(double timeScale) {
    planetLodCoordinator_.setTransitionDebugTimeScale(timeScale);
    config_.planetConfig.lodTransitionTimeScale = timeScale;
}

bool TerrainAppCore::isBlockingTerrainJobRunning() const {
    return terrainRegenerationRunning_;
}

bool TerrainAppCore::isTerrainJobRunning() const {
    return terrainRegenerationRunning_ || !planetPatchJobs_.empty() ||
        !readyPlanetUploads_.empty();
}

std::size_t TerrainAppCore::queuedPlanetPatchUploadCount() const {
    std::size_t result = 0;
    for (const PlanetPatchJob& job : planetPatchJobs_) {
        result += job.plan.upserts.size();
    }
    for (const PlanetPatchMeshBatch& batch : readyPlanetUploads_) {
        result += batch.upserts.size();
    }
    return result;
}

wgen::GeneratorPipelineSpec TerrainAppCore::currentPipeline() const {
    return pipelineSpec_;
}

wgen::Generator3dPipelineSpec TerrainAppCore::currentPlanetPipeline() const {
    return planetPipelineSpec_;
}

wgen::TerrainDisplayHeightRange TerrainAppCore::activePlanetDisplayHeightRange() const {
    if (activeTerrainField_ == nullptr) {
        return {};
    }
    return activeTerrainField_->displayHeightRange();
}

std::optional<TerrainJobResult> TerrainAppCore::tryTakeFinishedTerrainJob() {
    std::optional<TerrainJobResult> delivery;
    if (terrainRegenerationRunning_ && terrainGenerationJob_.valid() &&
            terrainGenerationJob_.wait_for(std::chrono::seconds{0}) ==
                std::future_status::ready) {
        TerrainJobResult result = terrainGenerationJob_.get();
        const bool replacesPlanetEpoch = result.planetBatch &&
            result.terrainEpoch != activeTerrainEpoch_;
        activeHeightMap_ = result.heightMap;
        activeTerrainField_ = result.terrainField;
        activeTerrainEpoch_ = result.terrainEpoch;
        terrainRegenerationRunning_ = false;
        planetRegenerationPending_ = false;
        if (result.planetBatch &&
                isCurrentPlanetPatchMeshBatch(
                    *result.planetBatch,
                    desiredPlanetRequestRevision_)) {
            if (replacesPlanetEpoch) {
                planetLodCoordinator_.reset();
                planetLodCoordinator_.setSurface(buildPlanetLodSurface(
                    *activeTerrainField_,
                    planetLodCoordinator_.lodConfig().patchQuadCount));
                planetLodSelectionDirty_ = true;
                planetLodSelectionElapsedSeconds_ = 0.0;
            }
            readyPlanetUploads_.push_back(std::move(*result.planetBatch));
        }
        result.planetBatch.reset();
        delivery = std::move(result);
    }

    pollFinishedPlanetPatchJobs();
    if (std::optional<PlanetPatchMeshBatch> batch = takeNextPlanetUploadBatch()) {
        if (!delivery) {
            delivery.emplace();
        }
        delivery->terrainEpoch = batch->terrainEpoch;
        delivery->planetBatch = std::move(*batch);
    }
    return delivery;
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
                wgen::MAX_TERRAIN_DETAIL_LEVEL)),
            .terrainDetail = wgen::Generator3dTerrainDetailSpec{
                .firstFullyVisibleLevel = static_cast<std::uint8_t>(std::min<std::size_t>(
                    n,
                    wgen::MAX_TERRAIN_DETAIL_LEVEL)),
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

TerrainPlaneMeshData TerrainAppCore::buildPlaneMeshData(
        const wgen::HeightMap<float>& heightMap) {
    TerrainPlaneMeshData data;
    appendHeightMapMesh(heightMap, -1.0F, 1.0F, -1.0F, 1.0F, data.vertices2d, data.indices2d);
    appendHeightMapMesh3d(heightMap, data.vertices3d, data.indices3d);
    return data;
}

PlanetPatchMeshBatch TerrainAppCore::buildPlanetPatchBatch(
        const wgen::TerrainFieldSnapshot& terrainField,
        const PlanetPatchVersion& version,
        const std::vector<wgen::PlanetPatchId>& upsertIds,
        const std::vector<wgen::PlanetPatchId>& removalIds,
        float skirtDepthMultiplier) {
    if (terrainField == nullptr) {
        throw std::invalid_argument("planet patch batch requires a terrain field snapshot");
    }
    if (!std::isfinite(skirtDepthMultiplier) || skirtDepthMultiplier < 0.0F) {
        throw std::invalid_argument("planet skirt depth multiplier must be finite and non-negative");
    }

    PlanetPatchMeshBatch batch{
        .terrainEpoch = version.terrainEpoch,
        .requestRevision = version.requestRevision,
    };
    const wgen::TerrainDetailPolicy& detailPolicy = terrainField->detailPolicy();
    batch.upserts.reserve(upsertIds.size());
    for (const wgen::PlanetPatchId& id : upsertIds) {
        const wgen::TerrainDetailLevel detail = detailPolicy.detailForCubeFacePatch(
            id.level,
            PLANET_PATCH_QUADS);
        const wgen::TerrainDetailLevel parentDetail = id.level == 0
            ? detail
            : detailPolicy.detailForCubeFacePatch(
                static_cast<std::uint8_t>(id.level - 1),
                PLANET_PATCH_QUADS);
        const wgen::TerrainHeightBounds& bounds = terrainField->heightBounds();
        const float conservativeDepth = std::max({
            bounds.maximumAbsoluteDisplacementMeters,
            bounds.omittedDetailErrorMeters(detail),
            terrainField->radius() * 0.000001F,
        });
        batch.upserts.push_back(buildRequestedPlanetPatchMesh(
            PlanetPatchMeshRequest{.id = id, .version = version},
            PLANET_PATCH_QUADS,
            terrainField->radius(),
            [&terrainField, detail](const wgen::PlanetSurfaceSample& surface) {
                return terrainField->sample(surface, detail);
            },
            PlanetPatchMeshBuildConfig{
                .skirtDepthMeters = conservativeDepth * skirtDepthMultiplier,
                .parentHeightSampler = [&terrainField, parentDetail](
                        const wgen::PlanetSurfaceSample& surface) {
                    return terrainField->sample(surface, parentDetail);
                },
            }));
    }
    batch.removals.reserve(removalIds.size());
    for (const wgen::PlanetPatchId& id : removalIds) {
        batch.removals.push_back({
            .id = id,
            .terrainEpoch = version.terrainEpoch,
            .requestRevision = version.requestRevision,
        });
    }
    validatePlanetPatchMeshBatch(batch);
    return batch;
}

wgen::PlanetLodSurface TerrainAppCore::buildPlanetLodSurface(
        const wgen::TerrainField& terrainField,
        std::uint32_t patchQuadCount) {
    const double inverseRadius = 1.0 / static_cast<double>(terrainField.radius());
    const wgen::TerrainHeightBounds& bounds = terrainField.heightBounds();
    wgen::PlanetLodSurface surface{
        .radius = 1.0,
        .minimumDisplacement =
            static_cast<double>(bounds.minimumDisplacementMeters) * inverseRadius,
        .maximumDisplacement =
            static_cast<double>(bounds.maximumDisplacementMeters) * inverseRadius,
    };
    for (std::uint8_t level = 0; level <= wgen::MAX_PLANET_PATCH_LEVEL; ++level) {
        const wgen::TerrainDetailLevel detail =
            terrainField.detailPolicy().detailForCubeFacePatch(level, patchQuadCount);
        surface.maximumOmittedDetailError[level] =
            static_cast<double>(bounds.omittedDetailErrorMeters(detail)) * inverseRadius;
    }
    return surface;
}

void TerrainAppCore::publishPlanetPatchBatch(const PlanetPatchMeshBatch& batch) {
    std::vector<wgen::PlanetPatchId> upserts;
    upserts.reserve(batch.upserts.size());
    for (const PlanetPatchMeshData& upsert : batch.upserts) {
        upserts.push_back(upsert.id);
    }
    std::vector<wgen::PlanetPatchId> removals;
    removals.reserve(batch.removals.size());
    for (const PlanetPatchRemoval& removal : batch.removals) {
        removals.push_back(removal.id);
    }
    std::unordered_map<
        wgen::PlanetPatchId,
        double,
        wgen::PlanetPatchIdHash> errorsByParent;
    if (activeTerrainField_ != nullptr) {
        const double inverseRadius = 1.0 /
            static_cast<double>(activeTerrainField_->radius());
        for (const PlanetPatchMeshData& upsert : batch.upserts) {
            const std::optional<wgen::PlanetPatchId> parentId =
                wgen::parent(upsert.id);
            if (!parentId) {
                continue;
            }
            errorsByParent[*parentId] = std::max(
                errorsByParent.contains(*parentId)
                    ? errorsByParent.at(*parentId)
                    : 0.0,
                static_cast<double>(upsert.maximumParentErrorMeters) *
                    inverseRadius);
        }
    }
    std::vector<wgen::PlanetPatchErrorSample> errorSamples;
    errorSamples.reserve(errorsByParent.size());
    for (const auto& [id, maximumWorldError] : errorsByParent) {
        errorSamples.push_back({id, maximumWorldError});
    }
    std::sort(errorSamples.begin(), errorSamples.end(), [](const auto& lhs, const auto& rhs) {
        return wgen::PlanetPatchIdLess{}(lhs.id, rhs.id);
    });
    planetLodCoordinator_.publishPatchChanges(
        upserts,
        removals,
        batch.requestRevision,
        errorSamples);
}

void TerrainAppCore::startPlanetLodJob(wgen::PlanetLodPatchPlan plan) {
    wgen::TerrainFieldSnapshot terrainField = activeTerrainField_;
    const std::uint64_t terrainEpoch = activeTerrainEpoch_;
    const std::uint64_t requestRevision = desiredPlanetRequestRevision_;
    const float skirtDepthMultiplier = config_.planetConfig.skirtDepthMultiplier;

    planetLodCoordinator_.beginPatchPlan(plan, requestRevision);
    PlanetPatchJob job{
        .plan = plan,
        .terrainEpoch = terrainEpoch,
        .requestRevision = requestRevision,
    };
    job.future = std::async(
        std::launch::async,
        [
            terrainField = std::move(terrainField),
            terrainEpoch,
            requestRevision,
            skirtDepthMultiplier,
            plan = std::move(plan)
        ]() mutable {
            return buildPlanetPatchBatch(
                terrainField,
                PlanetPatchVersion{
                    .terrainEpoch = terrainEpoch,
                    .dependencyRevision = 0,
                    .requestRevision = requestRevision,
                },
                plan.upserts,
                plan.removals,
                skirtDepthMultiplier);
        });
    planetPatchJobs_.push_back(std::move(job));
}

void TerrainAppCore::pollFinishedPlanetPatchJobs() {
    for (std::size_t index = 0; index < planetPatchJobs_.size();) {
        PlanetPatchJob& job = planetPatchJobs_[index];
        if (!job.future.valid() ||
                job.future.wait_for(std::chrono::seconds{0}) !=
                    std::future_status::ready) {
            ++index;
            continue;
        }

        try {
            PlanetPatchMeshBatch batch = job.future.get();
            if (job.terrainEpoch == activeTerrainEpoch_ &&
                    job.requestRevision == desiredPlanetRequestRevision_) {
                readyPlanetUploads_.push_back(std::move(batch));
            } else {
                planetLodCoordinator_.cancelPatchPlan(
                    job.plan,
                    job.requestRevision);
            }
        } catch (...) {
            planetLodCoordinator_.cancelPatchPlan(
                job.plan,
                job.requestRevision);
            planetPatchJobs_.erase(planetPatchJobs_.begin() + index);
            throw;
        }
        planetPatchJobs_.erase(planetPatchJobs_.begin() + index);
    }
}

void TerrainAppCore::discardStalePlanetUploads() {
    std::erase_if(readyPlanetUploads_, [this](const PlanetPatchMeshBatch& batch) {
        return batch.terrainEpoch != activeTerrainEpoch_ ||
            batch.requestRevision != desiredPlanetRequestRevision_;
    });
}

std::optional<PlanetPatchMeshBatch> TerrainAppCore::takeNextPlanetUploadBatch() {
    discardStalePlanetUploads();
    if (readyPlanetUploads_.empty()) {
        return std::nullopt;
    }

    PlanetPatchMeshBatch& source = readyPlanetUploads_.front();
    std::size_t uploadBudget =
        planetLodCoordinator_.streamingConfig().patchUploadBudget;
    if (planetLodCoordinator_.activeLeaves().empty() &&
            isCompletePlanetRootPatchBatch(source)) {
        // Replacing an epoch is a single six-root bootstrap operation. Keeping
        // it atomic prevents a selection revision from invalidating the tail
        // of the planet's only fallback coverage.
        uploadBudget = source.upserts.size();
    }
    PlanetPatchMeshBatch result = takePlanetPatchMeshBatchPrefix(
        source,
        uploadBudget);
    if (source.upserts.empty() && source.removals.empty()) {
        readyPlanetUploads_.pop_front();
    }
    if (result.upserts.empty() && result.removals.empty()) {
        return std::nullopt;
    }
    validatePlanetPatchMeshBatch(result);
    publishPlanetPatchBatch(result);
    return result;
}

std::function<glm::vec3(float)> TerrainAppCore::getActiveColorFunc() const {
    return ColorMapper::getColorFunction(activeColorFuncId_);
}

} // namespace lve
