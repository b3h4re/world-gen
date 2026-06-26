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

    bool checked_bool(
        const toml::node_view<const toml::node>& node,
        bool default_value,
        std::string_view name
    ) {
        auto value = node.value<bool>();

        if (!value) {
            if (node) {
                throw std::runtime_error(std::string{name} + " must be a boolean");
            }

            return default_value;
        }

        return *value;
    }

    float checked_float(
        const toml::node_view<const toml::node>& node,
        float default_value,
        std::string_view name
    ) {
        auto value = node.value<double>();

        if (!value) {
            if (node) {
                throw std::runtime_error(std::string{name} + " must be a float");
            }

            return default_value;
        }

        return static_cast<float>(*value);
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

        config.gridWidth = checked_uinteger<std::size_t>(
            root["terrain"]["perlin"]["grid_width"],
            config.gridWidth,
            "terrain.perlin.grid_width"
        );

        config.gridHeight = checked_uinteger<std::size_t>(
            root["terrain"]["perlin"]["grid_height"],
            config.gridHeight,
            "terrain.perlin.grid_height"
        );

        config.dotsPerCell = checked_uinteger<std::size_t>(
            root["terrain"]["perlin"]["dots_per_cell"],
            config.dotsPerCell,
            "terrain.perlin.dots_per_cell"
        );

        return config;
    }

    SimplexConfig parse_simplex_config(const toml::table& root) {
        SimplexConfig config;

        config.gridWidth = checked_uinteger<std::size_t>(
            root["terrain"]["simplex"]["grid_width"],
            config.gridWidth,
            "terrain.simplex.grid_width"
        );

        config.gridHeight = checked_uinteger<std::size_t>(
            root["terrain"]["simplex"]["grid_height"],
            config.gridHeight,
            "terrain.simplex.grid_height"
        );

        config.dotsPerCell = checked_uinteger<std::size_t>(
            root["terrain"]["simplex"]["dots_per_cell"],
            config.dotsPerCell,
            "terrain.simplex.dots_per_cell"
        );

        return config;
    }

    WaveletConfig parse_wavelet_config(const toml::table& root) {
        WaveletConfig config;

        config.gridWidth = checked_uinteger<std::size_t>(
            root["terrain"]["wavelet"]["grid_width"],
            config.gridWidth,
            "terrain.wavelet.grid_width"
        );

        config.gridHeight = checked_uinteger<std::size_t>(
            root["terrain"]["wavelet"]["grid_height"],
            config.gridHeight,
            "terrain.wavelet.grid_height"
        );

        return config;
    }

    WorleyConfig parse_worley_config(const toml::table& root) {
        WorleyConfig config;

        config.gridWidth = checked_uinteger<std::size_t>(
            root["terrain"]["worley"]["grid_width"],
            config.gridWidth,
            "terrain.worley.grid_width"
        );

        config.gridHeight = checked_uinteger<std::size_t>(
            root["terrain"]["worley"]["grid_height"],
            config.gridHeight,
            "terrain.worley.grid_height"
        );

        config.dotsPerCell = checked_uinteger<std::size_t>(
            root["terrain"]["worley"]["dots_per_cell"],
            config.dotsPerCell,
            "terrain.worley.dots_per_cell"
        );

        config.p = checked_float(
            root["terrain"]["worley"]["p"],
            config.p,
            "terrain.worley.p"
        );

        config.numPoints = checked_uinteger<std::size_t>(
            root["terrain"]["worley"]["num_points"],
            config.numPoints,
            "terrain.worley.num_points"
        );

        return config;
    }

    DLAConfig parse_dla_config(const toml::table& root) {
        DLAConfig config;

        config.numSteps = checked_uinteger<std::size_t>(
            root["terrain"]["dla"]["num_steps"],
            config.numSteps,
            "terrain.dla.num_steps"
        );

        config.fill = checked_float(
            root["terrain"]["dla"]["fill"],
            config.fill,
            "terrain.dla.fill"
        );

        config.jiggle = checked_float(
            root["terrain"]["dla"]["jiggle"],
            config.jiggle,
            "terrain.dla.jiggle"
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

        config.setSeed = checked_bool(
            root["terrain"]["set_seed"],
            config.setSeed,
            "terrain.set_seed"
        );

        config.width = checked_uinteger<std::size_t>(
            root["terrain"]["width"],
            config.width,
            "terrain.width"
        );

        config.height = checked_uinteger<std::size_t>(
            root["terrain"]["height"],
            config.height,
            "terrain.height"
        );

        config.perlin = parse_perlin_config(root);
        config.simplex = parse_simplex_config(root);
        config.wavelet = parse_wavelet_config(root);
        config.worley = parse_worley_config(root);
        config.dla = parse_dla_config(root);

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
