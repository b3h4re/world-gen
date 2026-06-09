cmake_minimum_required(VERSION 3.10.0)
project(world_gen VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/libs"
)


target_compile_features(world_gen PRIVATE cxx_std_20)
target_link_libraries(world_gen PRIVATE Vulkan::Vulkan glfw PNG::PNG)

include(CTest)
enable_testing()
