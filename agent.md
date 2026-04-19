# Agent Guide: MukkiGamesEngine

## Purpose
This file defines how I should implement tasks in this repository.
It is a Vulkan-based game engine with GLFW windowing, Dear ImGui tooling, and JSON-driven scene data.

## Project Snapshot
- Language target for new code: **C++17**
- Graphics API: **Vulkan**
- Window/input: **GLFW**
- UI/debug tools: **Dear ImGui** (+ ImGuizmo)
- Scene/config data: **nlohmann/json**
- Math: **GLM**
- Asset loading includes glTF workflow via loaders in `vulkan/Resources`

## Source Layout (working mental model)
- Entry point: `Main/MukkiGamesEngine/MukkiGamesEngine.cpp`
- App orchestration: `Main/MukkiGamesEngine/vulkan/Core/VkApplication.*`
- Vulkan core: `Main/MukkiGamesEngine/vulkan/Core/*`
- Rendering + command recording: `Main/MukkiGamesEngine/vulkan/*`
- Resources (models, textures, camera, scene): `Main/MukkiGamesEngine/vulkan/Resources/*`
- Descriptors: `Main/MukkiGamesEngine/vulkan/Descriptors/*`
- Compute + graphics pipelines: `Main/MukkiGamesEngine/vulkan/pipeline/*` and `Main/MukkiGamesEngine/vulkan/pipeline.*`
- UI manager: `Main/MukkiGamesEngine/vulkan/uiManager/*`
- Shaders: `Main/MukkiGamesEngine/Shaders/*`

## Implementation Rules

### 1) Language + Style
- Keep code compatible with **C++17** (no C++20-only features unless explicitly requested).
- Match existing naming/style in touched files (do not reformat entire files).
- Prefer small, focused edits over large refactors.
- Add comments only when logic is non-obvious.

### 2) Vulkan Safety
- Respect creation/destruction order and existing ownership patterns.
- On swapchain-dependent changes, ensure recreation path is handled (`recreateSwapChain`).
- Keep synchronization correct (fences, semaphores, per-frame/per-image semantics).
- Avoid hidden lifetime changes to `Vk*` resources unless task requires it.

### 3) Engine Architecture Expectations
- `VulkanApplication` is the top-level coordinator; keep responsibilities clear.
- Reuse existing managers (`BufferManager`, `TextureManager`, `CommandBufferManager`, `UIManager`) instead of duplicating logic.
- Scene-related features should flow through `SceneLoader` and scene JSON, not hardcoded values, unless clearly a debug fallback.

### 4) UI and Tools
- Debug/editor controls should integrate through `UIManager`.
- New runtime toggles should avoid breaking camera/input controls already mapped in `processInput()`.

### 5) Shaders and Assets
- If shader interfaces change, update both GLSL and C++ bindings/descriptors/pipeline layout.
- Preserve expected shader compilation flow from CMake and `ShaderCompiler`.
- Keep asset paths consistent with `ASSETS_PATH` usage.

## Task Workflow
1. Read affected modules first and map call flow before editing.
2. Implement minimal required changes in the most local place.
3. Check related code paths (init, frame loop, cleanup, swapchain recreate).
4. Build and report results with concrete errors if any.
5. Summarize changed files and why.

## Build and Verification
From `Main/`:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
```

If presets are unavailable in the current environment, use equivalent manual CMake configure/build commands and report what was used.

## Change Boundaries
- Do not introduce broad RAII/ownership refactors unless explicitly requested.
- Do not rename public engine types/functions without a strong reason.
- Do not remove debug utilities that are actively used by current workflows.

## Done Criteria for Any Task
- Code compiles (or compile issues are clearly explained with exact cause).
- New/changed behavior is wired into the correct runtime path.
- No obvious regressions in graphics/compute toggle, scene loading, or UI frame rendering.
- Cleanup path remains valid for added resources.
