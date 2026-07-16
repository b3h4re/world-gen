#include "app_config.hpp"
#include "terrain/planet/planet_lod_selector.hpp"
#include <toml++/toml.hpp>


#include <cmath>
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

        std::string mode = checked_string(
            root["window"]["present_mode"],
            "vsync",
            "window.present_mode"
        );
        if (mode == "vsync") {
            config.present_mode = lve::PresentMode::VSync;
        }
        if (mode == "immediate") {
            config.present_mode = lve::PresentMode::Immediate;
        }
        if (mode == "mailbox") {
            config.present_mode = lve::PresentMode::LowLatency;
        }


        config.fps_max = checked_integer<int>(
            root["window"]["fps_max"],
            config.fps_max,
            "window.fps_max"
        );

        return config;
    }

    PerlinConfig parse_perlin_config(const toml::table& root) {
        PerlinConfig config;

        config.dotsPerCell = checked_uinteger<std::size_t>(
            root["terrain"]["perlin"]["dots_per_cell"],
            config.dotsPerCell,
            "terrain.perlin.dots_per_cell"
        );

        return config;
    }

    SimplexConfig parse_simplex_config(const toml::table& root) {
        SimplexConfig config;

        config.dotsPerCell = checked_uinteger<std::size_t>(
            root["terrain"]["simplex"]["dots_per_cell"],
            config.dotsPerCell,
            "terrain.simplex.dots_per_cell"
        );

        return config;
    }

    WaveletConfig parse_wavelet_config(const toml::table& root) {
        WaveletConfig config;

        config.frequency = checked_float(
            root["terrain"]["wavelet"]["frequency"],
            config.frequency,
            "terrain.wavelet.frequency"
        );

        return config;
    }

    WorleyConfig parse_worley_config(const toml::table& root) {
        WorleyConfig config;

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

        config.heightFuncScale = checked_float(
            root["terrain"]["dla"]["height_scale"],
            config.heightFuncScale,
            "terrain.dla.height_scale"
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

        config.seed = checked_uinteger<SeedType>(
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

    PlanetConfig parse_planet_config(const toml::table& root) {
        PlanetConfig config;

        const auto heightModeNode = root["planet"]["height_mode"];
        if (!heightModeNode) {
            // Configuration files created before physical height semantics did
            // not contain this key and retain their normalized behavior.
            config.heightSemantics = TerrainHeightSemantics::LegacyNormalized;
        } else {
            const std::string heightMode = checked_string(
                heightModeNode,
                "physical_meters",
                "planet.height_mode"
            );
            if (heightMode == "physical_meters") {
                config.heightSemantics = TerrainHeightSemantics::PhysicalMeters;
            } else if (heightMode == "legacy_normalized") {
                config.heightSemantics = TerrainHeightSemantics::LegacyNormalized;
            } else {
                throw std::runtime_error(
                    "planet.height_mode must be 'physical_meters' or 'legacy_normalized'");
            }
        }

        config.resolution = checked_uinteger<std::size_t>(
            root["planet"]["resolution"],
            config.resolution,
            "planet.resolution"
        );
        if (config.resolution == 1) {
            throw std::runtime_error("planet.resolution must be 0 (automatic) or at least 2");
        }

        config.radius = checked_float(
            root["planet"]["radius"],
            config.radius,
            "planet.radius"
        );
        if (!std::isfinite(config.radius) || config.radius <= 0.0F) {
            throw std::runtime_error("planet.radius must be finite and positive");
        }

        config.skirtDepthMultiplier = checked_float(
            root["planet"]["skirt_depth_multiplier"],
            config.skirtDepthMultiplier,
            "planet.skirt_depth_multiplier"
        );
        if (!std::isfinite(config.skirtDepthMultiplier) ||
                config.skirtDepthMultiplier < 0.0F) {
            throw std::runtime_error("planet.skirt_depth_multiplier must be finite and non-negative");
        }

        config.lodTransitionDurationSeconds = checked_float(
            root["planet"]["lod_transition_seconds"],
            static_cast<float>(config.lodTransitionDurationSeconds),
            "planet.lod_transition_seconds"
        );
        if (!std::isfinite(config.lodTransitionDurationSeconds) ||
                config.lodTransitionDurationSeconds <= 0.0) {
            throw std::runtime_error("planet.lod_transition_seconds must be finite and positive");
        }

        config.lodTransitionTimeScale = checked_float(
            root["planet"]["lod_transition_time_scale"],
            static_cast<float>(config.lodTransitionTimeScale),
            "planet.lod_transition_time_scale"
        );
        if (!std::isfinite(config.lodTransitionTimeScale) ||
                config.lodTransitionTimeScale < 0.0) {
            throw std::runtime_error("planet.lod_transition_time_scale must be finite and non-negative");
        }

        config.lodSelectionIntervalSeconds = checked_float(
            root["planet"]["lod_selection_interval_seconds"],
            static_cast<float>(config.lodSelectionIntervalSeconds),
            "planet.lod_selection_interval_seconds"
        );
        if (!std::isfinite(config.lodSelectionIntervalSeconds) ||
                config.lodSelectionIntervalSeconds < 0.0) {
            throw std::runtime_error("planet.lod_selection_interval_seconds must be finite and non-negative");
        }

        config.lodPatchGenerationBudget = checked_uinteger<std::size_t>(
            root["planet"]["lod_patch_generation_budget"],
            config.lodPatchGenerationBudget,
            "planet.lod_patch_generation_budget"
        );
        config.lodPatchUploadBudget = checked_uinteger<std::size_t>(
            root["planet"]["lod_patch_upload_budget"],
            config.lodPatchUploadBudget,
            "planet.lod_patch_upload_budget"
        );
        config.lodMaximumConcurrentPatchJobs = checked_uinteger<std::size_t>(
            root["planet"]["lod_maximum_concurrent_patch_jobs"],
            config.lodMaximumConcurrentPatchJobs,
            "planet.lod_maximum_concurrent_patch_jobs"
        );
        if (config.lodPatchGenerationBudget < 4 ||
                config.lodPatchUploadBudget == 0 ||
                config.lodPatchUploadBudget >
                    DEFAULT_PLANET_RESIDENT_PATCH_BUDGET ||
                config.lodMaximumConcurrentPatchJobs == 0 ||
                config.lodMaximumConcurrentPatchJobs >
                    (DEFAULT_PLANET_RESIDENT_PATCH_BUDGET - FACES.size()) /
                        config.lodPatchGenerationBudget) {
            throw std::runtime_error("planet LOD streaming budgets are invalid or do not preserve fallback headroom");
        }

        const std::string computeMethod = checked_string(
            root["planet"]["compute_method"],
            "vulkan_compute",
            "planet.compute_method"
        );
        if (computeMethod == "cpu") {
            config.computeMethod = TerrainComputeMethod::Cpu;
        } else if (computeMethod == "vulkan_compute") {
            config.computeMethod = TerrainComputeMethod::VulkanCompute;
        } else {
            throw std::runtime_error("planet.compute_method must be 'cpu' or 'vulkan_compute'");
        }

        config.perlinCellSize = checked_float(
            root["planet"]["perlin_cell_size"],
            config.perlinCellSize,
            "planet.perlin_cell_size"
        );
        if (config.perlinCellSize <= 0.0F) {
            throw std::runtime_error("planet.perlin_cell_size must be positive");
        }

        config.octaves = checked_uinteger<std::size_t>(
            root["planet"]["octaves"],
            config.octaves,
            "planet.octaves"
        );

        config.lacunarity = checked_float(
            root["planet"]["lacunarity"],
            config.lacunarity,
            "planet.lacunarity"
        );
        if (config.lacunarity <= 0.0F) {
            throw std::runtime_error("planet.lacunarity must be positive");
        }

        config.persistence = checked_float(
            root["planet"]["persistence"],
            config.persistence,
            "planet.persistence"
        );
        if (config.persistence < 0.0F) {
            throw std::runtime_error("planet.persistence must be non-negative");
        }

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
        config.planetConfig = parse_planet_config(tbl);

        return config;

    }

}
