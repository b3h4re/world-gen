#include "app_config.hpp"
#include <toml++/toml.hpp>


#include <stdexcept>
#include <iostream>


namespace wgen {

    ProgramArgs parseArgs(int argc, char** argv) {
        ProgramArgs args{};


        for (int i = 1; i < argc; ++i) {
            std::string current = argv[i];
            if (current == "--config-path" || current == "-c") {
                if (i + 1 >= argc) {
                    throw std::runtime_error("Missing value after " + current);
                }
                args.configPath = files::Path{argv[++i]}.get();
            }
        }

        return args;
    }

    std::string checked_string(
        const toml::node_view<const toml::node>& node,
        std::string default_value,
        std::string_view name
    ) {
        auto value = node.value<std::string_view>();

        if (!value) {
            if (node) {
                throw std::runtime_error(std::string{name} + " must be a string");
            }

            return default_value;
        }

        return std::string{*value};
    }


    WindowConfig parse_window_config(const toml::table& root) {
        WindowConfig config;

        config.width = checked_integer<int>(
            root["window"]["width"],
            config.width,
            "window.width"
        );

        config.height = checked_integer<int>(
            root["window"]["height"],
            config.height,
            "window.height"
        );

        config.title = checked_string(
            root["window"]["title"],
            config.title,
            "window.title"
        );

        return config;
    }

    PerlinConfig parse_perlin_config(const toml::table& root) {
        PerlinConfig config;

        config.gridWidth = checked_integer<std::size_t>(
            root["terrain"]["perlin"]["gridWidth"],
            config.gridWidth,
            "terrain.perlin.gridWidth"
        );

        config.gridHeight = checked_integer<std::size_t>(
            root["terrain"]["perlin"]["gridHeight"],
            config.gridHeight,
            "terrain.perlin.gridHeight"
        );

        config.dotsPerCell = checked_integer<std::size_t>(
            root["terrain"]["perlin"]["dotsPerCell"],
            config.dotsPerCell,
            "terrain.perlin.dotsPerCell"
        );

        return config;
    }

    TerrainConfig parse_terrain_config(const toml::table& root) {
        TerrainConfig config;

        config.generator = checked_string(
            root["terrain"]["generator"],
            config.generator,
            "terrain.generator"
        );

        config.seed = checked_integer<std::uint32_t>(
            root["terrain"]["seed"],
            config.seed,
            "terrain.seed"
        );

        config.width = checked_integer<std::size_t>(
            root["terrain"]["width"],
            config.width,
            "terrain.width"
        );

        config.height = checked_integer<std::size_t>(
            root["terrain"]["height"],
            config.height,
            "terrain.height"
        );

        config.perlin = parse_perlin_config(root);

        return config;
    }


    AppConfig loadConfig(const std::filesystem::path &path) {
        AppConfig config{};
        files::Path configPath{path};
        if (!std::filesystem::exists(configPath.get())) {
            std::cout << "Config file not found at: " << configPath.string() << "  Loading defaults.\n";
            return config;
        }
        if (!std::filesystem::is_regular_file(configPath.get())) {
            throw std::runtime_error("Config path is not a regular file: " + configPath.string());
        }
        std::cout << "Config file found at: " << configPath.string() << "\n";

        toml::table tbl = toml::parse_file(configPath.string());

        config.windowConfig = parse_window_config(tbl);
        config.terrainConfig = parse_terrain_config(tbl);

        return config;

    }

}
