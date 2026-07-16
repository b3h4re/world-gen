#include "config/app_config.hpp"

#include "helpers.hpp"

#include <iostream>
#include <stdexcept>

#include <toml++/toml.hpp>

namespace {

toml::table parseToml(std::string_view source) {
    return toml::parse(source);
}

void testPlanetConfigDefaults() {
    wgen::tests::require(
        wgen::PlanetConfig{}.heightSemantics == wgen::TerrainHeightSemantics::PhysicalMeters,
        "new in-memory planet configs should use physical meter heights");
    const toml::table root = parseToml("");
    const wgen::PlanetConfig config = wgen::parse_planet_config(root);

    wgen::tests::require(config.resolution == 0, "default planet resolution should be automatic");
    wgen::tests::expectNear(config.radius, 100.0F, 0.00001F, "default planet radius is wrong");
    wgen::tests::require(
        config.heightSemantics == wgen::TerrainHeightSemantics::LegacyNormalized,
        "configuration files without height_mode should retain legacy height semantics");
    wgen::tests::require(
        config.computeMethod == wgen::TerrainComputeMethod::VulkanCompute,
        "default planet compute should be Vulkan compute");
    wgen::tests::expectNear(config.perlinCellSize, 1.0F, 0.00001F, "default planet cell size is wrong");
    wgen::tests::require(config.octaves == 5, "default planet octaves is wrong");
    wgen::tests::expectNear(config.lacunarity, 2.0F, 0.00001F, "default planet lacunarity is wrong");
    wgen::tests::expectNear(config.persistence, 0.5F, 0.00001F, "default planet persistence is wrong");
    wgen::tests::expectNear(
        config.skirtDepthMultiplier,
        1.0F,
        0.00001F,
        "default skirt depth multiplier is wrong");
    wgen::tests::expectNear(
        static_cast<float>(config.lodTransitionDurationSeconds),
        0.25F,
        0.00001F,
        "default LOD transition duration is wrong");
    wgen::tests::expectNear(
        static_cast<float>(config.lodTransitionTimeScale),
        1.0F,
        0.00001F,
        "default LOD transition speed is wrong");
}

void testPlanetConfigOverrides() {
    const toml::table root = parseToml(R"(
        [planet]
        resolution = 128
        radius = 42.5
        height_mode = "physical_meters"
        compute_method = "vulkan_compute"
        perlin_cell_size = 0.25
        octaves = 3
        lacunarity = 2.5
        persistence = 0.4
        skirt_depth_multiplier = 2.5
        lod_transition_seconds = 0.75
        lod_transition_time_scale = 0.2
    )");
    const wgen::PlanetConfig config = wgen::parse_planet_config(root);

    wgen::tests::require(config.resolution == 128, "planet resolution override is wrong");
    wgen::tests::expectNear(config.radius, 42.5F, 0.00001F, "planet radius override is wrong");
    wgen::tests::require(
        config.heightSemantics == wgen::TerrainHeightSemantics::PhysicalMeters,
        "planet height mode override is wrong");
    wgen::tests::require(config.computeMethod == wgen::TerrainComputeMethod::VulkanCompute, "planet compute override is wrong");
    wgen::tests::expectNear(config.perlinCellSize, 0.25F, 0.00001F, "planet cell size override is wrong");
    wgen::tests::require(config.octaves == 3, "planet octaves override is wrong");
    wgen::tests::expectNear(config.lacunarity, 2.5F, 0.00001F, "planet lacunarity override is wrong");
    wgen::tests::expectNear(config.persistence, 0.4F, 0.00001F, "planet persistence override is wrong");
    wgen::tests::expectNear(config.skirtDepthMultiplier, 2.5F, 0.00001F, "skirt multiplier override is wrong");
    wgen::tests::expectNear(
        static_cast<float>(config.lodTransitionDurationSeconds),
        0.75F,
        0.00001F,
        "LOD transition duration override is wrong");
    wgen::tests::expectNear(
        static_cast<float>(config.lodTransitionTimeScale),
        0.2F,
        0.00001F,
        "LOD transition speed override is wrong");
}

void testPlanetConfigRejectsBadComputeMethod() {
    const toml::table root = parseToml(R"(
        [planet]
        compute_method = "gpu"
    )");

    wgen::tests::requireThrows<std::runtime_error>(
        [&] { wgen::parse_planet_config(root); },
        "planet config should reject unknown compute method"
    );
}

void testPlanetConfigHeightCompatibility() {
    const toml::table legacy = parseToml(R"(
        [planet]
        height_mode = "legacy_normalized"
    )");
    wgen::tests::require(
        wgen::parse_planet_config(legacy).heightSemantics ==
            wgen::TerrainHeightSemantics::LegacyNormalized,
        "explicit legacy height mode should be supported");

    const toml::table invalid = parseToml(R"(
        [planet]
        height_mode = "centimeters"
    )");
    wgen::tests::requireThrows<std::runtime_error>(
        [&invalid] { wgen::parse_planet_config(invalid); },
        "planet config should reject unknown height semantics");
}

void testPlanetConfigRejectsBadValues() {
    const toml::table badResolution = parseToml(R"(
        [planet]
        resolution = 1
    )");
    wgen::tests::requireThrows<std::runtime_error>(
        [&] { wgen::parse_planet_config(badResolution); },
        "planet config should reject a face resolution of one"
    );

    const toml::table badCellSize = parseToml(R"(
        [planet]
        perlin_cell_size = 0.0
    )");
    wgen::tests::requireThrows<std::runtime_error>(
        [&] { wgen::parse_planet_config(badCellSize); },
        "planet config should reject non-positive cell size"
    );

    const toml::table badRadius = parseToml(R"(
        [planet]
        radius = 0.0
    )");
    wgen::tests::requireThrows<std::runtime_error>(
        [&] { wgen::parse_planet_config(badRadius); },
        "planet config should reject non-positive radius"
    );

    const toml::table badLacunarity = parseToml(R"(
        [planet]
        lacunarity = 0.0
    )");
    wgen::tests::requireThrows<std::runtime_error>(
        [&] { wgen::parse_planet_config(badLacunarity); },
        "planet config should reject non-positive lacunarity"
    );

    const toml::table badPersistence = parseToml(R"(
        [planet]
        persistence = -0.1
    )");
    wgen::tests::requireThrows<std::runtime_error>(
        [&] { wgen::parse_planet_config(badPersistence); },
        "planet config should reject negative persistence"
    );

    const toml::table badSkirtDepth = parseToml(R"(
        [planet]
        skirt_depth_multiplier = -1.0
    )");
    wgen::tests::requireThrows<std::runtime_error>(
        [&] { wgen::parse_planet_config(badSkirtDepth); },
        "planet config should reject a negative skirt multiplier");

    const toml::table badTransitionDuration = parseToml(R"(
        [planet]
        lod_transition_seconds = 0.0
    )");
    wgen::tests::requireThrows<std::runtime_error>(
        [&] { wgen::parse_planet_config(badTransitionDuration); },
        "planet config should reject a non-positive transition duration");

    const toml::table badTransitionTimeScale = parseToml(R"(
        [planet]
        lod_transition_time_scale = -0.1
    )");
    wgen::tests::requireThrows<std::runtime_error>(
        [&] { wgen::parse_planet_config(badTransitionTimeScale); },
        "planet config should reject a negative transition speed");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest("planet config defaults", testPlanetConfigDefaults);
        wgen::tests::runTest("planet config overrides", testPlanetConfigOverrides);
        wgen::tests::runTest("planet config rejects bad compute method", testPlanetConfigRejectsBadComputeMethod);
        wgen::tests::runTest("planet height compatibility", testPlanetConfigHeightCompatibility);
        wgen::tests::runTest("planet config rejects bad values", testPlanetConfigRejectsBadValues);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
