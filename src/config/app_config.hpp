#pragma once

#include "files/file_path.hpp"
#include "swap_chain/lve_swap_chain.hpp"
#include "terrain/generators/2d/generator.hpp"
#include "terrain/terrain_compute_method.hpp"
#include "terrain/terrain_height.hpp"

#include <string>
#include <cstdint>
#include <filesystem>
#include <toml++/toml.hpp>


namespace wgen {
    // command line arguments parser
    struct ProgramArgs{
        std::filesystem::path configPath = files::Path::homeConfigPath("world-gen", "config.toml").get();
    };

    ProgramArgs parseArgs(int argc, char** argv);

    // toml parser helper functions
    template <typename Int>
    Int checked_integer(
        const toml::node_view<const toml::node>& node,
        Int default_value,
        std::string_view name
    ) {
        static_assert(std::is_integral_v<Int>);

        auto value = node.value<int64_t>();

        if (!value) {
            if (node) {
                throw std::runtime_error(std::string{name} + " must be an integer");
            }

            return default_value;
        }

        if (*value < static_cast<int64_t>(std::numeric_limits<Int>::min()) ||
            *value > static_cast<int64_t>(std::numeric_limits<Int>::max())) {
            throw std::runtime_error(std::string{name} + " is out of range");
        }

        return static_cast<Int>(*value);
    }

    template <typename UInt>
    UInt checked_uinteger(
        const toml::node_view<const toml::node>& node,
        UInt default_value,
        std::string_view name
    ) {
        static_assert(std::is_integral_v<UInt>);

        auto value = node.value<uint64_t>();

        if (!value) {
            if (node) {
                throw std::runtime_error(std::string{name} + " must be an integer");
            }

            return default_value;
        }

        if (*value < static_cast<uint64_t>(std::numeric_limits<UInt>::min()) ||
            *value > static_cast<uint64_t>(std::numeric_limits<UInt>::max())) {
            throw std::runtime_error(std::string{name} + " is out of range");
        }

        return static_cast<UInt>(*value);
    }

    std::string checked_string(
        const toml::node_view<const toml::node>& node,
        std::string default_value,
        std::string_view name
    );

    bool checked_bool(
        const toml::node_view<const toml::node>& node,
        bool default_value,
        std::string_view name
    );

    float checked_float(
        const toml::node_view<const toml::node>& node,
        float default_value,
        std::string_view name
    );


    // app config struct
    struct WindowConfig {
        int width{1280};
        int height{720};
        lve::PresentMode present_mode = lve::PresentMode::VSync;
        int fps_max{144};
    };

    struct PerlinConfig {
        std::size_t dotsPerCell{100};
    };

    struct SimplexConfig {
        std::size_t dotsPerCell{100};
    };

    struct WaveletConfig {
        float frequency{0.014231234F};
    };

    struct WorleyConfig {
        std::size_t dotsPerCell{100};
        float p{2.0F};
        std::size_t numPoints{1};
    };

    struct DLAConfig {
        std::size_t numSteps{5};
        float heightFuncScale{0.15F};
        float fill{0.25F};
        float jiggle{0.021F};
    };

    struct TerrainConfig {
        std::string generator{"perlin"};
        bool setSeed = false;
        SeedType seed{0};
        std::size_t width{96};
        std::size_t height{64};
        PerlinConfig perlin{};
        SimplexConfig simplex{};
        WaveletConfig wavelet{};
        WorleyConfig worley{};
        DLAConfig dla{};
    };

    struct PlanetConfig {
        std::size_t resolution{0};
        // Planet radius and physical generator heights are authored in meters.
        float radius{100.0F};
        TerrainHeightSemantics heightSemantics{TerrainHeightSemantics::PhysicalMeters};
        float skirtDepthMultiplier{1.0F};
        double lodTransitionDurationSeconds{0.25};
        double lodTransitionTimeScale{1.0};
        TerrainComputeMethod computeMethod{TerrainComputeMethod::VulkanCompute};
        float perlinCellSize{1.0F};
        std::size_t octaves{5};
        float lacunarity{2.0F};
        float persistence{0.5F};
    };

    class AppConfig {
    public:
        WindowConfig windowConfig{};
        TerrainConfig terrainConfig{};
        PlanetConfig planetConfig{};

    };

    // parser functions
    WindowConfig parse_window_config(const toml::table& root);
    PerlinConfig parse_perlin_config(const toml::table& root);
    TerrainConfig parse_terrain_config(const toml::table& root);
    PlanetConfig parse_planet_config(const toml::table& root);

    AppConfig loadConfig(const std::filesystem::path &path);

}
