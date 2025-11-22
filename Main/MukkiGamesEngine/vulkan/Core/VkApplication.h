#pragma once
#include <vulkan/vulkan.h>
#include "VkInstance.h"
#include "VkDevice.h"
#include "EngineWindow.h"
#include "SwapChain.h"
#include "../pipeline.h"
#include "../RenderPass.h"
#include "../CommandBufferManager.h"
#include "../Descriptors/VkDescriptor.h"
#include <vector>

const int MAX_FRAMES_IN_FLIGHT = 2;

class VulkanApplication {
public:
	void run();
private:
	// Core components
	Instance instance;
	Window* window;
	Device* device;
	VulkanSwap* swapChain;
	
	// Rendering components
	VulkanRenderPass* renderPassObj;
	VkRenderPass renderPass;
	VulkanPipeline* graphicsPipeline;
	
	// Command buffers
	CommandBufferManager* commandBufferManager;
	
	// Synchronization objects
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	uint32_t currentFrame = 0;
	
	// Vertex/Index buffers (you'll need these)
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;
	uint32_t indexCount;
	
	// Descriptors
	VkDescriptorBoss* descriptorBoss;
	
	// Methods
	void initVulkan();
	void mainLoop();
	void cleanup();
	void createSyncObjects();
	void createVertexBuffer();
	void createIndexBuffer();
	void drawFrame();
	void recreateSwapChain();
};