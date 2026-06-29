cmake_minimum_required(VERSION 3.20)
project(world_gen VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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


add_executable(world_gen ${SOURCES} ${HEADERS})
target_include_directories(world_gen PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
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
    add_executable(perlin_tests
        tests/perlin_tests.cpp
        src/terrain/generators/generator.cpp
        src/terrain/generators/generator.hpp
        src/terrain/generators/noise/gradient_noise_interface.hpp
        src/terrain/generators/noise/perlin.cpp
        src/terrain/generators/noise/perlin.hpp
    )
    target_include_directories(perlin_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_features(perlin_tests PRIVATE cxx_std_20)
    add_test(NAME perlin_tests COMMAND perlin_tests)

    add_executable(heightmap_tests
        tests/heightmap_tests.cpp
    )
    target_include_directories(heightmap_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_features(heightmap_tests PRIVATE cxx_std_20)
    add_test(NAME heightmap_tests COMMAND heightmap_tests)

    add_executable(interpreter_tests
        tests/interpreter_tests.cpp
        src/scripts/intepreter.cpp
        src/scripts/interpreter.hpp
        src/files/file_path.cpp
        src/files/file_path.hpp
    )
    target_include_directories(interpreter_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_features(interpreter_tests PRIVATE cxx_std_20)
    add_test(NAME interpreter_tests COMMAND interpreter_tests)
endif()
