# CMakeList.txt : CMake project for MukkiGamesEngine, include source and define
# CMakeList.txt : CMake project for MukkiGamesEngine, include source and define
# project specific logic here.
#
find_package(Vulkan REQUIRED)

# Recursively find all source files in subdirectories
file(GLOB_RECURSE SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# Add source to this project's executable.
add_executable(MukkiGamesEngine ${SOURCES})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MukkiGamesEngine PROPERTY CXX_STANDARD 20)
endif()

# Link Vulkan (add other libraries as needed)
target_link_libraries(MukkiGamesEngine Vulkan::Vulkan)

# TODO: Add tests and install targets if needed.