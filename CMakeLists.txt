cmake_minimum_required(VERSION 3.28)
project(sirpg CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# 1. EnTT (v3.13.2)
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.2
)

# 2. Box2D (v3.0.0)
FetchContent_Declare(
    box2d
    GIT_REPOSITORY https://github.com/erincatto/box2d.git
    GIT_TAG v3.0.0
)

# 3. SDL3
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.8
)

# 4. GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

# 5. spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)

# Configure options for fetched projects to speed up builds
set(BOX2D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX2D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_DOCS OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(entt box2d SDL3 glm spdlog)

# Create executable
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.h"
    "src/*.hpp"
)

add_executable(sirpg ${SOURCES})

# Target-specific compile options for sirpg
if(MSVC)
    target_compile_options(sirpg PRIVATE /O2 /W4 /permissive-)
else()
    target_compile_options(sirpg PRIVATE -O3 -Wall -Wextra)
endif()

target_include_directories(sirpg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(sirpg PRIVATE
    EnTT::EnTT
    box2d
    SDL3::SDL3-static
    glm::glm
    spdlog::spdlog
)
