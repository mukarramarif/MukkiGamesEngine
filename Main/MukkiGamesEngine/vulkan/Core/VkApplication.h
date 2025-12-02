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
#include "ShaderCompiler.h"
const int MAX_FRAMES_IN_FLIGHT = 2;

class VulkanApplication {
public:
	// Explicit constructor and destructor
	VulkanApplication();
	~VulkanApplication();
	void run();
private:
	// Core components
	Instance instance;
	EngineWindow* window = nullptr;
	Device* device = nullptr;
	VulkanSwap* swapChain = nullptr;
	
	// Rendering components
	VulkanRenderPass* renderPassObj = nullptr;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VulkanPipeline* graphicsPipeline = nullptr;
	
	// Command buffers
	CommandBufferManager* commandBufferManager = nullptr;
	
	// Synchronization objects
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	uint32_t currentFrame = 0;
	
	// Vertex/Index buffers 
	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
	VkBuffer indexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
	uint32_t indexCount = 0;
	
	// Descriptors
	VkDescriptorBoss* descriptorBoss = nullptr;
	std::vector<VkDescriptorSet> descriptorSets;
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