cmake_minimum_required(VERSION 3.20)
project(world_gen VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/apps"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests"
)

find_package(Vulkan REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Gui Widgets)

include(FetchContent)
# Fetching toml++
fetchcontent_declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
fetchcontent_makeavailable(tomlplusplus)

# stb_truetype
fetchcontent_declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
)
fetchcontent_makeavailable(stb)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

# tinyobjloader
fetchcontent_declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc13
)
fetchcontent_makeavailable(tinyobjloader)

# Find glm
find_package(glm QUIET)

if(NOT glm_FOUND)
    include(FetchContent)

    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
    )

    FetchContent_MakeAvailable(glm)
endif()


# Adding source/headers
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE UI_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.ui"
)

add_executable(world_gen ${SOURCES} ${HEADERS})
target_sources(world_gen PRIVATE ${UI_FILES})
target_include_directories(world_gen PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/world_gen_autogen/include"
)


# Adding and compiling shaders
option(WORLD_GEN_PORTABLE_BUILD "Use relative runtime shader directory for packaged builds" OFF)

if(WORLD_GEN_PORTABLE_BUILD)
    set(WORLD_GEN_RUNTIME_SHADER_DIR "shaders")
else()
    set(WORLD_GEN_RUNTIME_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
endif()

target_compile_definitions(world_gen PRIVATE
    WORLD_GEN_SHADER_DIR="${WORLD_GEN_RUNTIME_SHADER_DIR}"
)

find_program(GLSLC glslc REQUIRED)

set(SHADER_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/include")

file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp"
)

# All include files
file(GLOB_RECURSE SHADER_INCLUDES CONFIGURE_DEPENDS
    "${SHADER_INCLUDE_DIR}/*.glsl"
)

set(SHADER_BINARY_FILES)

foreach(SHADER ${SHADER_SOURCES})

    file(RELATIVE_PATH SHADER_RELATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/shaders" "${SHADER}")

    set(SPIRV "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_RELATIVE_PATH}.spv")
    get_filename_component(SPIRV_DIRECTORY "${SPIRV}" DIRECTORY)

    add_custom_command(
        OUTPUT "${SPIRV}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_DIRECTORY}"

        COMMAND ${GLSLC}
            -I "${SHADER_INCLUDE_DIR}"
            "${SHADER}"
            -o "${SPIRV}"

        DEPENDS
            "${SHADER}"
            ${SHADER_INCLUDES}

        COMMENT "Compiling shader ${SHADER_RELATIVE_PATH}"
        VERBATIM
    )

    list(APPEND SHADER_BINARY_FILES "${SPIRV}")
endforeach()

add_custom_target(shaders DEPENDS ${SHADER_BINARY_FILES})

# Copying assets
add_custom_command(TARGET world_gen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:world_gen>/assets"
)


# Dependencies
add_dependencies(world_gen shaders)

# Linking
target_compile_features(world_gen PRIVATE cxx_std_20)
target_link_libraries(world_gen PRIVATE
    Vulkan::Vulkan
    glm::glm
    Qt6::Gui
    Qt6::Widgets
    tomlplusplus::tomlplusplus
    stb
    tinyobjloader
)
target_compile_definitions(world_gen PRIVATE WORLD_GEN_HAS_QT_BACKEND=1)

# Testing
include(CTest)
enable_testing()

if(BUILD_TESTING)
    set(TEST_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    # Perlin tests
    add_executable(perlin_tests
        tests/perlin_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/terrain/utils/hash_random.cpp
        src/terrain/utils/generator_utils.cpp
        src/terrain/generators/2d/generator.hpp
        src/terrain/generators/2d/noise/gradient_noise_interface.hpp
        src/terrain/generators/2d/noise/perlin.cpp
        src/terrain/generators/2d/noise/perlin.hpp
    )
    target_include_directories(perlin_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(perlin_tests PRIVATE cxx_std_20)
    add_test(NAME perlin_tests COMMAND perlin_tests)

    # HeightMap tests
    add_executable(heightmap_tests
        tests/heightmap_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
    )
    target_include_directories(heightmap_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(heightmap_tests PRIVATE cxx_std_20)
    add_test(NAME heightmap_tests COMMAND heightmap_tests)

    # Interpreter tests
    add_executable(interpreter_tests
        tests/interpreter_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/scripts/intepreter.cpp
        src/scripts/interpreter.hpp
        src/files/file_path.cpp
        src/files/file_path.hpp
        src/terrain/utils/hash_random.hpp
        src/terrain/utils/hash_random.cpp
        src/terrain/utils/generator_utils.cpp
        src/terrain/generators/2d/noise/worley.cpp
        src/terrain/generators/2d/noise/worley.hpp
        src/terrain/generators/2d/noise/wavelet.cpp
        src/terrain/generators/2d/noise/wavelet.hpp
        src/terrain/generators/2d/noise/value_noise.cpp
        src/terrain/generators/2d/noise/value_noise.hpp
        src/terrain/generators/2d/noise/simplex.cpp
        src/terrain/generators/2d/noise/simplex.hpp
        src/terrain/generators/2d/noise/perlin.cpp
        src/terrain/generators/2d/noise/perlin.hpp
        src/terrain/generators/2d/noise/layered_sin_noise.cpp
        src/terrain/generators/2d/noise/layered_sin_noise.hpp
    )
    target_include_directories(interpreter_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(interpreter_tests PRIVATE cxx_std_20)
    add_test(NAME interpreter_tests COMMAND interpreter_tests)

    # Terrain Pipeline Tests
    add_executable(terrain_pipeline_tests
        tests/terrain_pipeline_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/terrain/utils/hash_random.hpp
        src/terrain/utils/hash_random.cpp
        src/terrain/utils/generator_utils.cpp
        src/terrain/generators/2d/terrain_pipeline.cpp
        src/terrain/generators/2d/terrain_pipeline.hpp
        src/terrain/generators/2d/generator.hpp
        src/terrain/generators/2d/noise/value_noise.cpp
        src/terrain/generators/2d/noise/value_noise.hpp
    )
    target_include_directories(terrain_pipeline_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(terrain_pipeline_tests PRIVATE cxx_std_20)
    add_test(NAME terrain_pipeline_tests COMMAND terrain_pipeline_tests)

    # Terrain Pipeline 3D Tests
    add_executable(terrain_pipeline_3d_tests
        tests/terrain_pipeline_3d_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/terrain/utils/hash_random.hpp
        src/terrain/utils/hash_random.cpp
        src/terrain/utils/generator_utils.cpp
        src/terrain/generators/3d/generator.hpp
        src/terrain/generators/3d/generator_factory.cpp
        src/terrain/generators/3d/generator_factory.hpp
        src/terrain/generators/3d/generator_spec.hpp
        src/terrain/generators/3d/terrain_pipeline.cpp
        src/terrain/generators/3d/terrain_pipeline.hpp
        src/terrain/generators/3d/coordinate_scaled_generator.cpp
        src/terrain/generators/3d/coordinate_scaled_generator.hpp
        src/terrain/generators/3d/noise/perlin.cpp
        src/terrain/generators/3d/noise/perlin.hpp
        src/terrain/planet/cube_sphere.cpp
        src/terrain/planet/cube_sphere.hpp
    )
    target_include_directories(terrain_pipeline_3d_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(terrain_pipeline_3d_tests PRIVATE cxx_std_20)
    add_test(NAME terrain_pipeline_3d_tests COMMAND terrain_pipeline_3d_tests)

    # Planet patch topology tests
    add_executable(planet_patch_tests
        tests/planet_patch_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/terrain/planet/cube_sphere.cpp
        src/terrain/planet/cube_sphere.hpp
        src/terrain/planet/planet_patch.cpp
        src/terrain/planet/planet_patch.hpp
    )
    target_include_directories(planet_patch_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(planet_patch_tests PRIVATE cxx_std_20)
    add_test(NAME planet_patch_tests COMMAND planet_patch_tests)

    # Planet patch mesh tests
    add_executable(planet_patch_mesh_tests
        tests/planet_patch_mesh_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/terrain/planet/cube_sphere.cpp
        src/terrain/planet/cube_sphere.hpp
        src/terrain/planet/planet_patch.cpp
        src/terrain/planet/planet_patch.hpp
        src/terrain/planet/planet_patch_mesh.cpp
        src/terrain/planet/planet_patch_mesh.hpp
    )
    target_include_directories(planet_patch_mesh_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(planet_patch_mesh_tests PRIVATE cxx_std_20)
    target_link_libraries(planet_patch_mesh_tests PRIVATE glm::glm Vulkan::Vulkan)
    add_test(NAME planet_patch_mesh_tests COMMAND planet_patch_mesh_tests)

    # Planet patch batch tests
    add_executable(planet_patch_batch_tests
        tests/planet_patch_batch_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/terrain/planet/cube_sphere.cpp
        src/terrain/planet/cube_sphere.hpp
        src/terrain/planet/planet_patch.cpp
        src/terrain/planet/planet_patch.hpp
        src/terrain/planet/planet_patch_mesh.cpp
        src/terrain/planet/planet_patch_mesh.hpp
    )
    target_include_directories(planet_patch_batch_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(planet_patch_batch_tests PRIVATE cxx_std_20)
    target_link_libraries(planet_patch_batch_tests PRIVATE glm::glm Vulkan::Vulkan)
    add_test(NAME planet_patch_batch_tests COMMAND planet_patch_batch_tests)

    # Planet LOD selector tests
    add_executable(planet_lod_selector_tests
        tests/planet_lod_selector_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/game/camera/camera_3d.cpp
        src/game/camera/camera_3d.hpp
        src/terrain/planet/cube_sphere.cpp
        src/terrain/planet/cube_sphere.hpp
        src/terrain/planet/planet_patch.cpp
        src/terrain/planet/planet_patch.hpp
        src/terrain/planet/planet_lod_selector.cpp
        src/terrain/planet/planet_lod_selector.hpp
    )
    target_include_directories(planet_lod_selector_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(planet_lod_selector_tests PRIVATE cxx_std_20)
    target_link_libraries(planet_lod_selector_tests PRIVATE glm::glm)
    add_test(NAME planet_lod_selector_tests COMMAND planet_lod_selector_tests)

    # Planet LOD coordinator tests
    add_executable(planet_lod_coordinator_tests
        tests/planet_lod_coordinator_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/terrain/planet/cube_sphere.cpp
        src/terrain/planet/cube_sphere.hpp
        src/terrain/planet/planet_patch.cpp
        src/terrain/planet/planet_patch.hpp
        src/terrain/planet/planet_lod_selector.cpp
        src/terrain/planet/planet_lod_selector.hpp
        src/terrain/planet/planet_lod_coordinator.cpp
        src/terrain/planet/planet_lod_coordinator.hpp
    )
    target_include_directories(planet_lod_coordinator_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(planet_lod_coordinator_tests PRIVATE cxx_std_20)
    target_link_libraries(planet_lod_coordinator_tests PRIVATE glm::glm)
    add_test(NAME planet_lod_coordinator_tests COMMAND planet_lod_coordinator_tests)

    # Terrain field tests
    add_executable(terrain_field_tests
        tests/terrain_field_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/terrain/utils/hash_random.cpp
        src/terrain/utils/hash_random.hpp
        src/terrain/utils/generator_utils.cpp
        src/terrain/generators/3d/generator_factory.cpp
        src/terrain/generators/3d/generator_factory.hpp
        src/terrain/generators/3d/coordinate_scaled_generator.cpp
        src/terrain/generators/3d/coordinate_scaled_generator.hpp
        src/terrain/generators/3d/noise/perlin.cpp
        src/terrain/generators/3d/noise/perlin.hpp
        src/terrain/generators/3d/terrain_pipeline.cpp
        src/terrain/generators/3d/terrain_pipeline.hpp
        src/terrain/planet/cube_sphere.cpp
        src/terrain/planet/cube_sphere.hpp
        src/terrain/planet/terrain_field.cpp
        src/terrain/planet/terrain_field.hpp
    )
    target_include_directories(terrain_field_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(terrain_field_tests PRIVATE cxx_std_20)
    target_link_libraries(terrain_field_tests PRIVATE glm::glm)
    add_test(NAME terrain_field_tests COMMAND terrain_field_tests)

    # Terrain Pipeline Octave Tests
    add_executable(terrain_pipeline_octaves_tests
        tests/terrain_pipeline_octaves_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/terrain/utils/hash_random.hpp
        src/terrain/utils/hash_random.cpp
        src/terrain/utils/generator_utils.cpp
        src/terrain/generators/2d/coordinate_scaled_generator.cpp
        src/terrain/generators/2d/coordinate_scaled_generator.hpp
        src/terrain/generators/2d/generator.hpp
        src/terrain/generators/2d/generator_factory.cpp
        src/terrain/generators/2d/generator_factory.hpp
        src/terrain/generators/2d/generator_spec.hpp
        src/terrain/generators/2d/terrain_pipeline.cpp
        src/terrain/generators/2d/terrain_pipeline.hpp
        src/terrain/generators/2d/noise/gradient_noise_interface.hpp
        src/terrain/generators/2d/noise/noise_octaves.hpp
        src/terrain/generators/2d/noise/value_noise.cpp
        src/terrain/generators/2d/noise/value_noise.hpp
        src/terrain/generators/2d/noise/perlin.cpp
        src/terrain/generators/2d/noise/perlin.hpp
        src/terrain/generators/2d/noise/worley.cpp
        src/terrain/generators/2d/noise/worley.hpp
        src/terrain/generators/2d/noise/gabor.cpp
        src/terrain/generators/2d/noise/gabor.hpp
        src/terrain/generators/2d/noise/simplex.cpp
        src/terrain/generators/2d/noise/simplex.hpp
        src/terrain/generators/2d/noise/wavelet.cpp
        src/terrain/generators/2d/noise/wavelet.hpp
    )
    target_include_directories(terrain_pipeline_octaves_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(terrain_pipeline_octaves_tests PRIVATE cxx_std_20)
    add_test(NAME terrain_pipeline_octaves_tests COMMAND terrain_pipeline_octaves_tests)

    # Hash random tests
    add_executable(hash_uniform_real_distribution_tests
        tests/hash_uniform_real_distribution_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/terrain/utils/hash_random.cpp
        src/terrain/utils/hash_random.hpp
        src/terrain/utils/generator_utils.cpp
    )
    target_include_directories(hash_uniform_real_distribution_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(hash_uniform_real_distribution_tests PRIVATE cxx_std_20)
    add_test(NAME hash_uniform_real_distribution_tests COMMAND hash_uniform_real_distribution_tests)

    # Exporter tests
    add_executable(exporter_tests
        tests/exporter_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/files/exporter.cpp
        src/files/exporter.hpp
        src/files/file_path.cpp
        src/files/file_path.hpp
    )
    target_include_directories(exporter_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(exporter_tests PRIVATE cxx_std_20)
    target_link_libraries(exporter_tests PRIVATE stb glm::glm)
    add_test(NAME exporter_tests COMMAND exporter_tests)

    # App config tests
    add_executable(app_config_tests
        tests/app_config_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/config/app_config.cpp
        src/config/app_config.hpp
        src/files/file_path.cpp
        src/files/file_path.hpp
        src/terrain/utils/hash_random.cpp
        src/terrain/utils/hash_random.hpp
        src/terrain/utils/generator_utils.cpp
    )
    target_include_directories(app_config_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(app_config_tests PRIVATE cxx_std_20)
    target_link_libraries(app_config_tests PRIVATE
        glm::glm
        Qt6::Gui
        tomlplusplus::tomlplusplus
    )
    add_test(NAME app_config_tests COMMAND app_config_tests)


    # GPU compute tests
    add_executable(gpu_terrain_generation_tests
        tests/gpu_terrain_generation_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/device/lve_device.cpp
        src/device/lve_device.hpp
        src/device/lve_compute_device.cpp
        src/device/lve_compute_device.hpp
        src/model/buffer/lve_buffer.cpp
        src/model/buffer/lve_buffer.hpp
        src/pipeline/descriptors/lve_descriptors.cpp
        src/pipeline/descriptors/lve_descriptors.hpp
        src/terrain/utils/hash_random.cpp
        src/terrain/utils/hash_random.hpp
        src/terrain/utils/generator_utils.cpp
        src/terrain/generators/2d/coordinate_scaled_generator.cpp
        src/terrain/generators/2d/coordinate_scaled_generator.hpp
        src/terrain/generators/2d/generator.hpp
        src/terrain/generators/2d/generator_factory.cpp
        src/terrain/generators/2d/generator_factory.hpp
        src/terrain/generators/2d/generator_spec.hpp
        src/terrain/generators/2d/terrain_pipeline.cpp
        src/terrain/generators/2d/terrain_pipeline.hpp
        src/terrain/generators/2d/noise/value_noise.cpp
        src/terrain/generators/2d/noise/value_noise.hpp
        src/terrain/generators/2d/noise/gradient_noise_interface.hpp
        src/terrain/generators/2d/noise/perlin.cpp
        src/terrain/generators/2d/noise/perlin.hpp
        src/terrain/generators/2d/noise/worley.cpp
        src/terrain/generators/2d/noise/worley.hpp
        src/terrain/generators/2d/noise/simplex.cpp
        src/terrain/generators/2d/noise/simplex.hpp
        src/terrain/generators/2d/noise/wavelet.cpp
        src/terrain/generators/2d/noise/wavelet.hpp
        src/terrain/generators/2d/noise/gabor.cpp
        src/terrain/generators/2d/noise/gabor.hpp
        src/terrain/generators/3d/generator.hpp
        src/terrain/generators/3d/generator_factory.cpp
        src/terrain/generators/3d/generator_factory.hpp
        src/terrain/generators/3d/generator_spec.hpp
        src/terrain/generators/3d/terrain_pipeline.cpp
        src/terrain/generators/3d/terrain_pipeline.hpp
        src/terrain/generators/3d/coordinate_scaled_generator.cpp
        src/terrain/generators/3d/coordinate_scaled_generator.hpp
        src/terrain/generators/3d/noise/perlin.cpp
        src/terrain/generators/3d/noise/perlin.hpp
        src/terrain/planet/cube_sphere.cpp
        src/terrain/planet/cube_sphere.hpp
        src/renderer/compute/computer.cpp
        src/renderer/compute/computer.hpp
        src/renderer/compute/gpu_height_map.cpp
        src/renderer/compute/gpu_height_map.hpp
        src/renderer/compute/gpu_planet_generator.cpp
        src/renderer/compute/gpu_planet_generator.hpp
        src/renderer/compute/gpu_planet_pipeline.cpp
        src/renderer/compute/gpu_planet_pipeline.hpp
        src/renderer/compute/gpu_generator.cpp
        src/renderer/compute/gpu_generator.hpp
        src/renderer/compute/gpu_height_map_accumulator.cpp
        src/renderer/compute/gpu_height_map_accumulator.hpp
        src/renderer/compute/gpu_terrain_pipeline.cpp
        src/renderer/compute/gpu_terrain_pipeline.hpp
    )
    target_include_directories(gpu_terrain_generation_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(gpu_terrain_generation_tests PRIVATE cxx_std_20)
    target_compile_definitions(gpu_terrain_generation_tests PRIVATE
    WORLD_GEN_SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders"
)
	    target_link_libraries(gpu_terrain_generation_tests PRIVATE
            glm::glm
	        Vulkan::Vulkan
	    )
	    add_dependencies(gpu_terrain_generation_tests shaders)
	    add_test(NAME gpu_terrain_generation_tests COMMAND gpu_terrain_generation_tests)
    set_tests_properties(gpu_terrain_generation_tests PROPERTIES SKIP_RETURN_CODE 77)
endif()
