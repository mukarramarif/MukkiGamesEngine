# MukkiGamesEngine
Renderer Project following a rewrite of the original Vulkan tutorial. General-purpose renderer for learning and implementing new rendering techniques. This project is designed for rendering multiple glTF objects and multiple lights using principal BRDF techniques in real-time. 
I will keep adding new rendering techniques here 😁
## Progress

| Type of Rendering | Techniques | Images |
| ----------- | ------------ | ------------ |
| RayTracing  | Glass Rendering Iridescene| <img width="1709" height="1321" alt="Screenshot 2026-08-24 170027" src="https://github.com/user-attachments/assets/5e74dad3-087e-4609-a507-e6719f555ab8" /> <img width="2337" height="1368" alt="Screenshot 2026-08-24 124139" src="https://github.com/user-attachments/assets/a8c3809d-bbb6-47e1-88e9-91c386d64ffa" />|
|			  |        Dispersion          | <img width="1431" height="912" alt="Screenshot 2026-08-20 201023" src="https://github.com/user-attachments/assets/73930297-7b1a-48fa-898a-9416d6ea2ffa" />
|			| Emissive Coal | <img width="2503" height="1326" alt="image" src="https://github.com/user-attachments/assets/5bcad8db-4291-46e5-a392-0581e1ab3eba" />|
|			| Shadows from directional and point lights | <img width="1405" height="1335" alt="Screenshot 2026-08-13 180242" src="https://github.com/user-attachments/assets/93605918-fd5c-4bea-8263-cc2c728faff2" />| 
|           | GPU Mesh instancing                               | <img width="2493" height="962" alt="Screenshot 2026-09-01 142150" src="https://github.com/user-attachments/assets/07c0890e-8909-4080-87cb-d760cc38fe6f" /> 
| RayMarching | Volumetric Clouds(still experimental) | <img width="969" height="933" alt="Screenshot 2026-08-11 230604" src="https://github.com/user-attachments/assets/04b9f660-90f7-4055-98c5-2a2c3988d9a5" /> <img width="1671" height="1237" alt="Screenshot 2026-08-11 234059" src="https://github.com/user-attachments/assets/22f6c1dd-c842-4adf-a84d-3e9bc746ea94" /> <img width="1537" height="1006" alt="Screenshot 2026-08-08 135051" src="https://github.com/user-attachments/assets/3d1ceb1e-574a-4a41-ab3e-9427e514cf21" />
| Rasterization | Shadows | <img width="1377" height="868" alt="image" src="https://github.com/user-attachments/assets/57b37e72-8c3d-4592-a70a-0bd81c77bafe" /> <img width="1569" height="1175" alt="Screenshot 2026-08-13 201745" src="https://github.com/user-attachments/assets/9b9b382e-45c9-4276-a13f-941f7461f3ad" /> |
|             | BRDF Basic Shine |  <img width="2347" height="866" alt="Screenshot 2026-07-16 181129" src="https://github.com/user-attachments/assets/3e85b0fc-a9b4-483b-91f8-00d55163a58b" /> <img width="1558" height="1348" alt="image" src="https://github.com/user-attachments/assets/32bfceeb-bcfc-42e4-bcdb-bc2d0bddc939" />







## Build (Windows)
### Prerequisites
- **CMake 3.31.4+** (3.16 minimum) and **Ninja**
- **Conan 2.x**
- **Vulkan SDK** (LunarG) for headers/loader + `glslc`
- **Slang** compiler for `slangc` (or `SLANG_ROOT` set)
- **Visual Studio** with MSVC toolchain + Windows SDK

### Configure & build (Release)
> Run these commands from the **x64 Native Tools Command Prompt for VS** so `cl.exe` is available.

```sh
cd Main

conan profile detect --force

# Recommended: use Conan preset with Ninja
conan install . -of build --build=missing -s build_type=Release -s compiler.cppstd=20 -c tools.cmake.cmaketoolchain:generator=Ninja
cmake --preset conan-release
cmake --build --preset conan-release
```

If you prefer not to use presets, configure directly:

```sh
cmake -S . -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Debug build
Use a separate build folder for Debug to avoid mixing configs:

```sh
cd Main

conan install . -of build-debug --build=missing -s build_type=Debug -s compiler.cppstd=20 -c tools.cmake.cmaketoolchain:generator=Ninja
cmake --preset conan-debug
cmake --build --preset conan-debug
# go into build folder you adjust backends in the future and run the sample scene to load first 
cd build-debug
./MukkiGamesEngine --backend vulkan --scene ../MukkiGamesEngine/Assets/sceneTrack.json

```


## Current Progress
- [x] abstract code from tracer rounds for basic instance and device setup
- [x] setup Vulkan instance and device
- [x] create window with GLFW
- [x] create Vulkan surface with GLFW
- [x] setup swapchain
- [x] create image views for swapchain images
- [x] create render pass
- [x] create framebuffers
- [x] create command pool and command buffers
- [x] create synchronization objects
- [x] basic rendering loop to clear screen with a color
- [x] render 3d objects
- [x] skybox
- [x] render cubmaps
- [x] create scene loader
    - [x] SceneObject
    - [ ] Shader hot reloading
- [x] Basic raytracing pipeline (TLAS/BLAS, SBT, rgen/rmiss/rchit shaders)
- [x] RT lighting with shadows and BRDF
	- [x] BRDF
 	- [x] Shadows	
- [x] RT cubemap skydome fallback
- [ ] RenderGraph
	- [x] Basic Rewrite for VKapplication
 	- [ ] Json format for renderpasses
- [ ] Adding GLTF 2.0 Support
	- [x] Glass Dispersion
 	- [x] Glass Iridescence
  	- [x] Specular Support
  	- [x] Transmission Support
  	- [x] GPU mesh instancing
  	- [x] IOR
  	- [ ] Mesh Shader Support
- [ ] looking into SIMD
- [ ] multithreading for rendering and resource loading
	- [x] std::async for model loading
 	- [ ] subcommandbuffers? 
## Fixes
- [x] recreating swapchain on window resize
- [x] validation layers errors when switching from compute to graphics and back
- [x] Abstract Vulkan calls to a draw function that we pass the scene path too.
- [x] RAII cleanup (tech depth)
	- [x] adding unique_ptrs to member variables in VkApplication
 	- [x] fixing dangling ptrs in code after switching scenes
## Architecture Diagram
```mermaid
flowchart TD
  vulkan--> renderer
  subgraph renderer
	   subgraph core
	   		commandPool[Command Pool]
		swapchain[Swapchain]
		renderPass[Render Pass]
		framebuffers[Framebuffers]
		syncObjects[Synchronization Objects]
	   end
	   subgraph resources
		models[Models]
		textures[Textures]
		shaders[Shaders]
	   end
	   subgraph scene
		sceneLoader[Scene Loader]
		entities[Entities]
	   end
	   subgraph raytracing
		raytracingAS[RT Acceleration Structures]
		raytracingPipeline[RT Pipeline + SBT]
		raytracingShader[RT Shaders]
	   end
	   subgraph ui
		uiRenderer[UI Renderer]
	   end
  end
```


