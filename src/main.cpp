#include "apps/terrain_app.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

int main() {
    try {
        lve::TerrainApp app{};
        app.run();
    } catch (const std::exception &exception) {
        std::cerr << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
