#pragma once

#include "files/file_path.hpp"

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


    // app config struct
    struct WindowConfig {
        int width{1280};
        int height{720};
        std::string title{"World Generator"};
    };

    struct PerlinConfig {
        std::size_t gridWidth{100};
        std::size_t gridHeight{100};
        std::size_t dotsPerCell{100};
    };

    struct TerrainConfig {
        std::string generator{"perlin"};
        std::uint32_t seed{12345};
        std::size_t width{96};
        std::size_t height{64};
        PerlinConfig perlin{};
    };

    class AppConfig {
    public:
        WindowConfig windowConfig{};
        TerrainConfig terrainConfig{};

    };

    // parser functions
    WindowConfig parse_window_config(const toml::table& root);
    PerlinConfig parse_perlin_config(const toml::table& root);
    TerrainConfig parse_terrain_config(const toml::table& root);

    AppConfig loadConfig(const std::filesystem::path &path);

}
