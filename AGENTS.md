# AGENTS.md

This file contains guidelines and commands for agentic coding agents working on the MukkiGamesEngine project.

## Project Overview

MukkiGamesEngine is a C++20 Vulkan-based game engine project. The codebase follows a modular architecture with clear separation between core Vulkan components, resource management, and rendering systems.

## Build System

This project uses CMake as the build system. The main CMakeLists.txt is located in `Main/MukkiGamesEngine/`.

### Build Commands

```bash
# Configure and build from root directory
cmake -S Main/MukkiGamesEngine -B build
cmake --build build

# Alternative: Build directly
cd Main/MukkiGamesEngine
mkdir build && cd build
cmake ..
cmake --build .
```

### Running the Application

```bash
# From build directory
./MukkiGamesEngine

# From root directory
build/MukkiGamesEngine
```

### Testing

Currently no formal test framework is implemented. Manual testing is done by running the application and verifying rendering output.

## Code Style Guidelines

### File Organization

- Headers use `.h` extension, source files use `.cpp`
- Header guards use `#pragma once` (preferred) or traditional `#ifndef` pattern
- Directory structure follows functional organization:
  - `vulkan/Core/` - Core Vulkan components (instance, device, window)
  - `vulkan/Resources/` - Resource managers (buffers, textures, camera)
  - `vulkan/Descriptors/` - Descriptor management
  - `vulkan/pipeline/` - Graphics and compute pipelines
  - `vulkan/uiManager/` - UI management

### Naming Conventions

- **Classes**: PascalCase (e.g., `EngineWindow`, `VulkanApplication`)
- **Functions**: camelCase (e.g., `initVulkan`, `createVertexBuffer`)
- **Variables**: camelCase for member variables, snake_case for local variables
- **Constants**: UPPER_SNAKE_CASE (e.g., `MAX_FRAMES_IN_FLIGHT`)
- **Private members**: Prefix with underscore or use `m_` prefix consistently
- **Vulkan handles**: Use descriptive names (e.g., `vertexBuffer`, `depthImageView`)

### Import/Include Style

```cpp
// System headers first
#include <stdexcept>
#include <vector>
#include <iostream>

// External libraries (GLFW, Vulkan, GLM)
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

// Project headers - use relative paths
#include "EngineWindow.h"
#include "../objects/vertex.h"
```

### Formatting and Spacing

- Use 4 spaces for indentation (no tabs)
- Opening braces on same line for functions/methods, new line for classes
- Space after keywords: `if (condition)`, `while (true)`
- No space between function name and parentheses: `function()`
- Pointer/reference notation: `Type* name` and `Type& name` (aligned to type)

### Error Handling

- Use exceptions for error handling (`std::runtime_error`, `std::logic_error`)
- Check all Vulkan function return values
- Use descriptive error messages with context
- Resource cleanup in destructors or dedicated cleanup methods

```cpp
if (result != VK_SUCCESS) {
    std::ostringstream os;
    os << "glfwCreateWindowSurface failed (code=" << result << ")";
    throw std::runtime_error(os.str());
}
```

### Memory Management

- Use RAII principles - wrap resources in classes with proper destructors
- Prefer smart pointers where appropriate, but raw pointers are acceptable for Vulkan objects
- Follow Vulkan object lifecycle: create → use → destroy in reverse order
- Initialize handles to `VK_NULL_HANDLE`

### Vulkan-Specific Guidelines

- Use `VK_NULL_HANDLE` for uninitialized Vulkan handles
- Follow the Vulkan initialization order strictly
- Use descriptive variable names for Vulkan objects
- Group related Vulkan objects in structs or classes
- Use `constexpr` for fixed-size arrays and constants

### Documentation

- Use clear, descriptive function and variable names
- Add comments for complex Vulkan operations or non-obvious code
- Document member variable purposes in class definitions
- Use `//` for single-line comments, `/* */` for multi-line comments

### Shader Management

- Shaders are compiled to SPIR-V at build time
- Use `ShaderCompiler::compileShadersIfNeeded()` to ensure shaders are available
- Shader files are located in `Shaders/` directory
- Compiled SPIR-V files should be next to executable at runtime

### Performance Considerations

- Use staging buffers for vertex/index data transfer
- Prefer device-local memory for frequently accessed GPU resources
- Implement proper synchronization for multi-frame rendering
- Use command buffers efficiently - record once, submit many times when possible

## Common Patterns

### Resource Initialization Pattern
```cpp
void initResource() {
    // 1. Create staging buffer (CPU visible)
    // 2. Map memory and copy data
    // 3. Create GPU-local buffer
    // 4. Copy from staging to GPU buffer
    // 5. Cleanup staging buffer
}
```

### Cleanup Pattern
```cpp
~Class() {
    cleanup();
}

void cleanup() {
    if (resource != VK_NULL_HANDLE) {
        vkDestroyResource(device, resource, nullptr);
        resource = VK_NULL_HANDLE;
    }
}
```

### Error Checking Pattern
```cpp
VkResult result = vkFunction(...);
if (result != VK_SUCCESS) {
    throw std::runtime_error("vkFunction failed");
}
```

## Dependencies

- **Vulkan**: Core graphics API
- **GLFW**: Window management and input
- **GLM**: Mathematics library for 3D graphics
- **CMake**: Build system
- **C++20**: Modern C++ features

## Platform Support

- Primary target: Windows (uses Windows-specific code in ShaderCompiler)
- Linux support partially implemented in ShaderCompiler

## Development Workflow

1. Make changes to source files
2. Build using CMake commands
3. Test by running the application
4. Verify rendering output and functionality
5. Clean up resources properly in destructors

## Notes for Agents

- Always check Vulkan function return values
- Follow the established cleanup patterns to avoid resource leaks
- Use the existing class structure and naming conventions
- Test shader compilation if modifying shader-related code
- Be mindful of the multi-frame rendering synchronization