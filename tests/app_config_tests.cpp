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
    const toml::table root = parseToml("");
    const wgen::PlanetConfig config = wgen::parse_planet_config(root);

    wgen::tests::require(config.resolution == 0, "default planet resolution should be automatic");
    wgen::tests::expectNear(config.radius, 100.0F, 0.00001F, "default planet radius is wrong");
    wgen::tests::require(
        config.computeMethod == wgen::TerrainComputeMethod::VulkanCompute,
        "default planet compute should be Vulkan compute");
    wgen::tests::expectNear(config.perlinCellSize, 1.0F, 0.00001F, "default planet cell size is wrong");
    wgen::tests::require(config.octaves == 5, "default planet octaves is wrong");
    wgen::tests::expectNear(config.lacunarity, 2.0F, 0.00001F, "default planet lacunarity is wrong");
    wgen::tests::expectNear(config.persistence, 0.5F, 0.00001F, "default planet persistence is wrong");
}

void testPlanetConfigOverrides() {
    const toml::table root = parseToml(R"(
        [planet]
        resolution = 128
        radius = 42.5
        compute_method = "vulkan_compute"
        perlin_cell_size = 0.25
        octaves = 3
        lacunarity = 2.5
        persistence = 0.4
    )");
    const wgen::PlanetConfig config = wgen::parse_planet_config(root);

    wgen::tests::require(config.resolution == 128, "planet resolution override is wrong");
    wgen::tests::expectNear(config.radius, 42.5F, 0.00001F, "planet radius override is wrong");
    wgen::tests::require(config.computeMethod == wgen::TerrainComputeMethod::VulkanCompute, "planet compute override is wrong");
    wgen::tests::expectNear(config.perlinCellSize, 0.25F, 0.00001F, "planet cell size override is wrong");
    wgen::tests::require(config.octaves == 3, "planet octaves override is wrong");
    wgen::tests::expectNear(config.lacunarity, 2.5F, 0.00001F, "planet lacunarity override is wrong");
    wgen::tests::expectNear(config.persistence, 0.4F, 0.00001F, "planet persistence override is wrong");
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
}

} // namespace

int main() {
    try {
        wgen::tests::runTest("planet config defaults", testPlanetConfigDefaults);
        wgen::tests::runTest("planet config overrides", testPlanetConfigOverrides);
        wgen::tests::runTest("planet config rejects bad compute method", testPlanetConfigRejectsBadComputeMethod);
        wgen::tests::runTest("planet config rejects bad values", testPlanetConfigRejectsBadValues);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
