#pragma once
#include "Core/VkInstance.h"  // Changed from Instance.h
#include "Core/VkDevice.h"
#include "Core/SwapChain.h"
#include "Core/EngineWindow.h"
#include "Descriptors/VkDescriptor.h"
#include "RenderPass.h"
#include "pipeline.h"
#include <vector>
#include <set>
#include <iostream>

const int MAX_FRAMES_IN_FLIGHT = 2;

class VulkanRenderer {
private:
    // Core Vulkan objects
    Instance instance;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    Device* device = nullptr;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    // Command infrastructure
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // Renderpass & Pipeline
	VulkanPipeline *graphicsPipeline = nullptr;
	VkRenderPass renderPass;
	VulkanRenderPass* renderPassObj = nullptr;
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

	// Descriptor Sets
	VkDescriptorBoss* descriptorBoss = nullptr;
  VkDescriptorSetLayout descriptorSetLayout;
    // Synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    std::set<uint32_t> uniqueQueueFamilies;
    uint32_t graphicsQueueFamilyIndex;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
public:
    // Initialization
    void init(EngineWindow* window);
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();

    // Command setup
    void createCommandPool();
    void createCommandBuffers();

    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    // Pipeline setup
    void createRenderPass();
    VkShaderModule createShaderModule(const std::vector<char>& code);

    // Resource management
    void createTexture(const char* filename);
    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();

    void createDescriptorSetLayout();

    void createSyncObjects();
    // Rendering
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void drawFrame();
    VkSurfaceKHR getSurface();
    // Cleanup
    void cleanup();

};