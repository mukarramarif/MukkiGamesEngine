cmake_minimum_required(VERSION 3.16)

project(MukkiGamesEngine LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
file(GLOB ENGINE_SOURCES
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/Core/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/Core/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/Descriptors/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/Descriptors/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/SceneLoader/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/SceneLoader/*.h"
)

add_executable(MukkiGamesEngine ${ENGINE_SOURCES})

set_property(TARGET MukkiGamesEngine PROPERTY CXX_STANDARD 20)
set_property(TARGET MukkiGamesEngine PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(MukkiGamesEngine PRIVATE Vulkan::Vulkan glfw imgui::imgui glm::glm)

