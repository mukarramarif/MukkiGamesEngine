cmake_minimum_required(VERSION 3.16)

project(MukkiGamesEngine LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")

file(GLOB ENGINE_SOURCES
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/Core/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/Core/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/Descriptors/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/Descriptors/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/Resources/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/Resources/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/uiManager/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/uiManager/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/pipeline/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/pipeline/*.h"
)   
message(STATUS "Engine sources: ${ENGINE_SOURCES}")
add_executable(MukkiGamesEngine ${ENGINE_SOURCES})

set_property(TARGET MukkiGamesEngine PROPERTY CXX_STANDARD 20)
set_property(TARGET MukkiGamesEngine PROPERTY CXX_STANDARD_REQUIRED ON)
#libraries
target_link_libraries(MukkiGamesEngine PRIVATE Vulkan::Vulkan glfw imgui::imgui glm::glm)
#header libraries
target_include_directories(MukkiGamesEngine PRIVATE ${TINYGLTF_INCLUDE_DIRS})
# Helper to compile GLSL to SPIR-V. Usage: add_shaders(<target> Shaders/foo.vert Shaders/bar.frag)
target_compile_definitions(MukkiGamesEngine PRIVATE 
    ASSETS_PATH="${CMAKE_SOURCE_DIR}/MukkiGamesEngine/Assets/")

function(add_shaders TARGET_NAME)
  set(SHADER_SOURCE_FILES ${ARGN})
  list(LENGTH SHADER_SOURCE_FILES FILE_COUNT)
  if(FILE_COUNT EQUAL 0)
    message(FATAL_ERROR "Cannot create a shaders target without any source files")
  endif()

  set(SHADER_PRODUCTS)
  foreach(SHADER_SOURCE IN LISTS SHADER_SOURCE_FILES)
    # Get the filename without path (e.g., "shader.vert")
    get_filename_component(SHADER_NAME "${SHADER_SOURCE}" NAME)
    
    # Output name will be <build-dir>/<name>.spv -> shader.vert.spv, skybox.vert.spv
    set(OUT "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.spv")
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

# Compile all shaders
add_shaders(MukkiGamesEngine 
    ../Main/MukkiGamesEngine/Shaders/shader.vert 
    ../Main/MukkiGamesEngine/Shaders/shader.frag 
    ../Main/MukkiGamesEngine/Shaders/compute.comp 
    ../Main/MukkiGamesEngine/Shaders/skybox.vert 
    ../Main/MukkiGamesEngine/Shaders/skybox.frag
)


