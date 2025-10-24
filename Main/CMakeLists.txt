cmake_minimum_required(VERSION 3.16) # Add this at the top to fix the warning

project(MukkiGamesEngine LANGUAGES CXX) # Specify language to fix FindThreads error

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED) # Add ImGui if using vcpkg

file(GLOB ENGINE_SOURCES
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/*.h"
)

add_executable(MukkiGamesEngine ${ENGINE_SOURCES})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MukkiGamesEngine PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(MukkiGamesEngine PRIVATE Vulkan::Vulkan glfw imgui::imgui)
