cmake_minimum_required(VERSION 3.20)
project(world_gen VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

# Fetching toml++
include(FetchContent)
fetchcontent_declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
fetchcontent_makeavailable(tomlplusplus)


# Adding source/headers
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_executable(world_gen ${SOURCES} ${HEADERS})

target_include_directories(world_gen PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(world_gen PRIVATE cxx_std_20)
target_link_libraries(world_gen PRIVATE
    Vulkan::Vulkan
    glfw
    tomlplusplus::tomlplusplus
)

# Adding and compiling shaders
target_compile_definitions(world_gen PRIVATE
    WORLD_GEN_SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders"
)

find_program(GLSLC glslc REQUIRED)

file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag"
)

set(SHADER_BINARY_FILES)
foreach(SHADER ${SHADER_SOURCES})
    file(RELATIVE_PATH SHADER_RELATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/shaders" "${SHADER}")
    set(SPIRV "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_RELATIVE_PATH}.spv")
    get_filename_component(SPIRV_DIRECTORY "${SPIRV}" DIRECTORY)

    add_custom_command(
        OUTPUT "${SPIRV}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_DIRECTORY}"
        COMMAND ${GLSLC} "${SHADER}" -o "${SPIRV}"
        DEPENDS "${SHADER}"
        COMMENT "Compiling shader ${SHADER_RELATIVE_PATH}"
        VERBATIM
    )

    list(APPEND SHADER_BINARY_FILES "${SPIRV}")
endforeach()

add_custom_target(shaders DEPENDS ${SHADER_BINARY_FILES})
add_dependencies(world_gen shaders)

# Testing
include(CTest)
enable_testing()

if(BUILD_TESTING)
    add_executable(perlin_tests
        tests/perlin_tests.cpp
        src/terrain/generators/noise/perlin.cpp
    )
    target_include_directories(perlin_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_features(perlin_tests PRIVATE cxx_std_20)
    add_test(NAME perlin_tests COMMAND perlin_tests)
endif()
