#include "apps/terrain_app.hpp"
#include "config/app_config.hpp"

#include <toml++/toml.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>



int main(int argc, char** argv) {
    wgen::ProgramArgs args = wgen::parseArgs(argc, argv);


    wgen::AppConfig config;

    try {
        config = wgen::loadConfig(args.configPath);
    } catch (const std::exception &exception) {
        std::cerr << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    try {
        lve::TerrainApp app{};
        app.run();
    } catch (const std::exception &exception) {
        std::cerr << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
