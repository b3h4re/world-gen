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
target_compile_definitions(world_gen PRIVATE
    WORLD_GEN_SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders"
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
    Qt6::Gui
    Qt6::Widgets
    tomlplusplus::tomlplusplus
    stb
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
        src/terrain/generators/generator.cpp
        src/terrain/generators/generator.hpp
        src/terrain/generators/noise/gradient_noise_interface.hpp
        src/terrain/generators/noise/perlin.cpp
        src/terrain/generators/noise/perlin.hpp
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
        src/terrain/generators/generator.cpp
        src/terrain/generators/noise/worley.cpp
        src/terrain/generators/noise/worley.hpp
        src/terrain/generators/noise/wavelet.cpp
        src/terrain/generators/noise/wavelet.hpp
        src/terrain/generators/noise/value_noise.cpp
        src/terrain/generators/noise/value_noise.hpp
        src/terrain/generators/noise/simplex.cpp
        src/terrain/generators/noise/simplex.hpp
        src/terrain/generators/noise/perlin.cpp
        src/terrain/generators/noise/perlin.hpp
        src/terrain/generators/noise/layered_sin_noise.cpp
        src/terrain/generators/noise/layered_sin_noise.hpp
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
        src/terrain/generators/terrain_pipeline.cpp
        src/terrain/generators/terrain_pipeline.hpp
        src/terrain/generators/generator.cpp
        src/terrain/generators/generator.hpp
        src/terrain/generators/noise/value_noise.cpp
        src/terrain/generators/noise/value_noise.hpp
    )
    target_include_directories(terrain_pipeline_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(terrain_pipeline_tests PRIVATE cxx_std_20)
    add_test(NAME terrain_pipeline_tests COMMAND terrain_pipeline_tests)

    # Hash random tests
    add_executable(hash_uniform_real_distribution_tests
        tests/hash_uniform_real_distribution_tests.cpp
        tests/helpers.cpp
        tests/helpers.hpp
        src/terrain/utils/hash_random.cpp
        src/terrain/utils/hash_random.hpp
    )
    target_include_directories(hash_uniform_real_distribution_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(hash_uniform_real_distribution_tests PRIVATE cxx_std_20)
    add_test(NAME hash_uniform_real_distribution_tests COMMAND hash_uniform_real_distribution_tests)


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
        src/terrain/generators/generator.cpp
        src/terrain/generators/generator.hpp
        src/terrain/generators/noise/value_noise.cpp
        src/terrain/generators/noise/value_noise.hpp
        src/terrain/generators/noise/gradient_noise_interface.hpp
        src/terrain/generators/noise/perlin.cpp
        src/terrain/generators/noise/perlin.hpp
        src/renderer/compute/computer.cpp
        src/renderer/compute/computer.hpp
        src/renderer/compute/gpu_height_map.cpp
        src/renderer/compute/gpu_height_map.hpp
    )
    target_include_directories(gpu_terrain_generation_tests PRIVATE ${TEST_INCLUDE_DIRECTORIES})
    target_compile_features(gpu_terrain_generation_tests PRIVATE cxx_std_20)
    target_compile_definitions(gpu_terrain_generation_tests PRIVATE
    WORLD_GEN_SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders"
)
    target_link_libraries(gpu_terrain_generation_tests PRIVATE
        Vulkan::Vulkan
    )
    add_test(NAME gpu_terrain_generation_tests COMMAND gpu_terrain_generation_tests)
    set_tests_properties(gpu_terrain_generation_tests PROPERTIES SKIP_RETURN_CODE 77)
endif()
