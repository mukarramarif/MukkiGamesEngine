cmake_minimum_required(VERSION 3.16)

if (CMAKE_VERSION VERSION_LESS 3.31.4)
  message(FATAL_ERROR "MukkiGamesEngine requires CMake 3.31.4 or newer.")
endif()

if (NOT CMAKE_GENERATOR STREQUAL "Ninja")
  message(FATAL_ERROR "MukkiGamesEngine requires the Ninja generator. Re-run CMake with -G Ninja.")
endif()

project(MukkiGamesEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(stb CONFIG REQUIRED)
find_package(gli CONFIG REQUIRED)

find_program(GLSLC_EXECUTABLE glslc
  HINTS ENV VULKAN_SDK
  PATH_SUFFIXES Bin bin
)

if (NOT GLSLC_EXECUTABLE)
  message(FATAL_ERROR "glslc not found. Install the Vulkan SDK or shaderc and ensure glslc is on PATH.")
endif()

find_program(SLANGC_EXECUTABLE slangc
  HINTS ENV SLANG_ROOT
  PATH_SUFFIXES Bin bin
)

if (NOT SLANGC_EXECUTABLE)
  message(FATAL_ERROR "slangc not found. Install Slang and ensure slangc is on PATH or SLANG_ROOT is set.")
endif()

find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
if (NOT TINYGLTF_INCLUDE_DIRS)
  message(FATAL_ERROR "tiny_gltf.h not found. Install tinygltf or set TINYGLTF_INCLUDE_DIRS.")
endif()

# Recursively find all source files in subdirectories
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"

)

# Add source to this project's executable.
add_executable(MukkiGamesEngine ${SOURCES})

# Link Vulkan and other libraries
target_link_libraries(MukkiGamesEngine PRIVATE Vulkan::Vulkan glfw imgui::imgui glm::glm imguizmo::imguizmo nlohmann_json::nlohmann_json stb::stb gli)

# header libraries
target_include_directories(MukkiGamesEngine PRIVATE ${TINYGLTF_INCLUDE_DIRS})

# Helper to compile GLSL to SPIR-V. Usage: add_shaders(<target> Shaders/foo.vert Shaders/bar.frag)
target_compile_definitions(MukkiGamesEngine PRIVATE
    ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Assets/")

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
      COMMAND "${GLSLC_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}" -o "${OUT}"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}"
      COMMENT "Compiling shader ${SHADER_SOURCE} -> ${OUT}"
      VERBATIM
    )
    list(APPEND SHADER_PRODUCTS "${OUT}")
  endforeach()

  # Create a target that builds all shader products and make the main target depend on it
  add_custom_target(${TARGET_NAME}_shaders ALL DEPENDS ${SHADER_PRODUCTS})
  add_dependencies(${TARGET_NAME} ${TARGET_NAME}_shaders)
endfunction()

function(add_slang_fragment_shaders TARGET_NAME)
  set(SHADER_SOURCE_FILES ${ARGN})
  list(LENGTH SHADER_SOURCE_FILES FILE_COUNT)
  if(FILE_COUNT EQUAL 0)
    message(FATAL_ERROR "Cannot create a Slang shaders target without any source files")
  endif()

  set(SHADER_PRODUCTS)
  foreach(SHADER_SOURCE IN LISTS SHADER_SOURCE_FILES)
    get_filename_component(SHADER_BASE "${SHADER_SOURCE}" NAME_WE)

    # Output name will be <build-dir>/<basename>.frag.spv -> brdf.frag.spv
    set(OUT "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_BASE}.frag.spv")
    add_custom_command(
      OUTPUT "${OUT}"
      COMMAND "${SLANGC_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}"
        -target spirv -profile spirv_1_5 -entry main -stage fragment -o "${OUT}"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}"
      COMMENT "Compiling Slang shader ${SHADER_SOURCE} -> ${OUT}"
      VERBATIM
    )
    list(APPEND SHADER_PRODUCTS "${OUT}")
  endforeach()

  add_custom_target(${TARGET_NAME}_slang_shaders ALL DEPENDS ${SHADER_PRODUCTS})
  add_dependencies(${TARGET_NAME} ${TARGET_NAME}_slang_shaders)
endfunction()

# Compile all shaders
add_shaders(MukkiGamesEngine
    Shaders/shader.vert
    Shaders/compute.comp
    Shaders/skybox.vert
    Shaders/skybox.frag
)

add_slang_fragment_shaders(MukkiGamesEngine
    Shaders/brdf.slang
)

# TODO: Add tests and install targets if needed.
