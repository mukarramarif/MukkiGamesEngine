#pragma once
#include "Core/VkInstance.h"  // Changed from Instance.h
#include "Core/VkDevice.h"
#include "Core/SwapChain.h"
#include "Core/EngineWindow.h"
#include <vector>
#include <set>
#include <iostream>
class VkInstance;
class Device;

class VulkanRenderer {
private:
    // Core Vulkan objects
    Instance instance;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    Device device = Device(physicalDevice,surface);
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    // Command infrastructure
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // Renderpass & Pipeline
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    // Shaders
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;

    // Textures & Resources
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;

    // Swapchain (for presentation)
	VulkanSwap swapChain;

    // Synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::set<uint32_t> uniqueQueueFamilies;
    uint32_t graphicsQueueFamilyIndex;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
public:
    // Initialization
    void init(Window* window);
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();

    // Command setup
    void createCommandPool();
    void createCommandBuffers();

    // Pipeline setup
    void createRenderPass();
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void createGraphicsPipeline();

    // Resource management
    void createTexture(const char* filename);
    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();

    // Rendering
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void drawFrame();
    VkSurfaceKHR getSurface();
    // Cleanup
    void cleanup();

};