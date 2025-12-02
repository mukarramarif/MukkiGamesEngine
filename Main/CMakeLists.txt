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

# Helper to compile GLSL to SPIR-V. Usage: add_shaders(<target> Shaders/foo.vert Shaders/bar.frag)
function(add_shaders TARGET_NAME)
  set(SHADER_SOURCE_FILES ${ARGN})
  list(LENGTH SHADER_SOURCE_FILES FILE_COUNT)
  if(FILE_COUNT EQUAL 0)
    message(FATAL_ERROR "Cannot create a shaders target without any source files")
  endif()

  set(SHADER_PRODUCTS)
  foreach(SHADER_SOURCE IN LISTS SHADER_SOURCE_FILES)
    # Get extension name without leading dot: "vert" or "frag"
    get_filename_component(SHADER_EXT "${SHADER_SOURCE}" EXT)
    string(REGEX REPLACE "^\\." "" SHADER_EXT "${SHADER_EXT}")

    # Output name will be <build-dir>/<ext>.spv -> vert.spv, frag.spv
    set(OUT "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_EXT}.spv")
    add_custom_command(
      OUTPUT "${OUT}"
      COMMAND Vulkan::glslc "${CMAKE_SOURCE_DIR}/${SHADER_SOURCE}" -o "${OUT}"
      DEPENDS "${CMAKE_SOURCE_DIR}/${SHADER_SOURCE}"
      COMMENT "Compiling shader ${SHADER_SOURCE} -> ${OUT}"
      VERBATIM
    )
    list(APPEND SHADER_PRODUCTS "${OUT}")
  endforeach()

  # Create a target that builds all shader products and make the main target depend on it
  add_custom_target(${TARGET_NAME}_shaders ALL DEPENDS ${SHADER_PRODUCTS})
  add_dependencies(${TARGET_NAME} ${TARGET_NAME}_shaders)
endfunction()

# Example: compile Shaders/shader.vert -> vert.spv and Shaders/shader.frag -> frag.spv
add_shaders(MukkiGamesEngine Shaders/shader.vert Shaders/shader.frag)

