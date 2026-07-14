cmake_minimum_required(VERSION 3.31.4)

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

file(GLOB ENGINE_SOURCES CONFIGURE_DEPENDS
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
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/objects/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/utils/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/utils/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/uiManager/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/uiManager/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/uiManager/backends/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/uiManager/backends/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/pipeline/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/pipeline/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/Physics/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/Physics/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/Renderer/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/Renderer/*.h"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/raytracing/*.cpp"
    "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/raytracing/*.h"
)

# Jolt Physics
include(FetchContent)
FetchContent_Declare(
  JoltPhysics
  GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
  GIT_TAG v5.5.0
  SOURCE_SUBDIR Build
)
set(JOLT_ENABLE_LUA_BINDINGS OFF)
FetchContent_MakeAvailable(JoltPhysics)

# Jolt Physics targets C++17, ensure compatibility
set_target_properties(Jolt PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(MSVC)
	set_target_properties(Jolt PROPERTIES
		MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>$<$<NOT:$<CONFIG:Debug>>:MultiThreadedDLL>")
endif()

message(STATUS "Engine sources: ${ENGINE_SOURCES}")
add_executable(MukkiGamesEngine ${ENGINE_SOURCES})

# --- clang-tidy integration (opt-in) ---
option(ENABLE_CLANG_TIDY "Run clang-tidy on source files during build" OFF)
if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXECUTABLE clang-tidy
    HINTS ENV LLVM_DIR
    PATH_SUFFIXES bin
  )
  if(NOT CLANG_TIDY_EXECUTABLE)
    message(FATAL_ERROR "ENABLE_CLANG_TIDY is ON but clang-tidy was not found. "
                        "Install LLVM/clang-tidy or set LLVM_DIR.")
  endif()
  message(STATUS "clang-tidy found: ${CLANG_TIDY_EXECUTABLE}")
  set_target_properties(MukkiGamesEngine PROPERTIES
    CXX_CLANG_TIDY
    "${CLANG_TIDY_EXECUTABLE};-p=${CMAKE_BINARY_DIR};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
  )
endif()
# ----------------------------------------

# libraries
target_link_libraries(MukkiGamesEngine PRIVATE Vulkan::Vulkan glfw imgui::imgui glm::glm imguizmo::imguizmo nlohmann_json::nlohmann_json stb::stb gli Jolt)

# imgui backend headers (imgui_impl_*) are installed to <package_root>/res/bindings
get_filename_component(_IMGUI_ROOT ${imgui_INCLUDE_DIR} DIRECTORY)
target_include_directories(MukkiGamesEngine PRIVATE "${_IMGUI_ROOT}/res")
target_sources(MukkiGamesEngine PRIVATE
    "${_IMGUI_ROOT}/res/bindings/imgui_impl_vulkan.cpp"
    "${_IMGUI_ROOT}/res/bindings/imgui_impl_glfw.cpp"
)

# header libraries
target_include_directories(MukkiGamesEngine PRIVATE ${TINYGLTF_INCLUDE_DIRS})
target_include_directories(MukkiGamesEngine PRIVATE "${CMAKE_SOURCE_DIR}/MukkiGamesEngine/vulkan/uiManager/backends")

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
    set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
    file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")
    set(OUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    set(SHADER_ARGS "${CMAKE_SOURCE_DIR}/${SHADER_SOURCE}" -o "${OUT}")

    if(SHADER_NAME MATCHES "\\.rgen$")
      list(APPEND SHADER_ARGS -fshader-stage=rgen --target-env=vulkan1.4 --target-spv=spv1.4)
    elseif(SHADER_NAME MATCHES "\\.rmiss$")
      list(APPEND SHADER_ARGS -fshader-stage=rmiss --target-env=vulkan1.4 --target-spv=spv1.4)
    elseif(SHADER_NAME MATCHES "\\.rchit$")
      list(APPEND SHADER_ARGS -fshader-stage=rchit --target-env=vulkan1.4 --target-spv=spv1.4)
    endif()

    add_custom_command(
      OUTPUT "${OUT}"
      COMMAND "${GLSLC_EXECUTABLE}" ${SHADER_ARGS}
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

function(add_slang_fragment_shaders TARGET_NAME)
  set(SHADER_SOURCE_FILES ${ARGN})
  list(LENGTH SHADER_SOURCE_FILES FILE_COUNT)
  if(FILE_COUNT EQUAL 0)
    message(FATAL_ERROR "Cannot create a Slang shaders target without any source files")
  endif()

  set(SHADER_PRODUCTS)
  foreach(SHADER_SOURCE IN LISTS SHADER_SOURCE_FILES)
    get_filename_component(SHADER_BASE "${SHADER_SOURCE}" NAME_WE)

    # Output name will be <build-dir>/Shaders/<basename>.frag.spv -> Shaders/brdf.frag.spv
    set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
    file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")
    set(OUT "${SHADER_OUTPUT_DIR}/${SHADER_BASE}.frag.spv")
    add_custom_command(
      OUTPUT "${OUT}"
      COMMAND "${SLANGC_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/${SHADER_SOURCE}"
        -target spirv -profile spirv_1_5 -entry main -stage fragment -o "${OUT}"
      DEPENDS "${CMAKE_SOURCE_DIR}/${SHADER_SOURCE}"
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
    MukkiGamesEngine/Shaders/shader.vert
    MukkiGamesEngine/Shaders/shadow.vert
    MukkiGamesEngine/Shaders/shadow.frag
    MukkiGamesEngine/Shaders/compute.comp
    MukkiGamesEngine/Shaders/skybox.vert
    MukkiGamesEngine/Shaders/skybox.frag
    MukkiGamesEngine/Shaders/rt.rgen
    MukkiGamesEngine/Shaders/rt.rmiss
    MukkiGamesEngine/Shaders/rt.rchit
    MukkiGamesEngine/Shaders/taa.comp
)

add_slang_fragment_shaders(MukkiGamesEngine
    MukkiGamesEngine/Shaders/brdf.slang
)


# TODO: Add tests and install targets if needed.
