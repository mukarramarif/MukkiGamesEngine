#include "VkApplication.h"

void VulkanApplication::run()
{
	initVulkan();
	mainLoop();
	cleanup();
}

void VulkanApplication::initVulkan()
{
	instance.createInstance();
	window = new Window();
	window->init(800, 600, "Mukki Games Engine");
	
	
	VkSurfaceKHR surface = window->createSurface(instance.getInstance());
	
	
	device = new Device(instance.getInstance(), surface);
	

	swapChain = new VulkanSwap();
	swapChain->initSwap(*device, surface, window->getGLFWwindow());
	swapChain->createImageViews();
	
	
	renderPassObj = new VulkanRenderPass(device, swapChain->getSwapChainImageFormat());
	renderPass = renderPassObj->getRenderPass();
	

	swapChain->createFramebuffers(renderPass);
	
	
	graphicsPipeline = new VulkanPipeline(
		device, 
		renderPass, 
		"shaders/vert.spv", 
		"shaders/frag.spv", 
		window, 
		&instance
	);
	
	
	commandBufferManager = new CommandBufferManager();
	commandBufferManager->init(*device, MAX_FRAMES_IN_FLIGHT);
	

	createVertexBuffer();
	createIndexBuffer();
	
	
	descriptorBoss = new VkDescriptorBoss();
	descriptorBoss->init(
		device, 
		graphicsPipeline->getDescriptorSetLayout(), 
		MAX_FRAMES_IN_FLIGHT
	);
	

	createSyncObjects();
}

void VulkanApplication::createSyncObjects()
{
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(device->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(device->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(device->getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create synchronization objects!");
		}
	}
}

	

void VulkanApplication::createVertexBuffer()
{
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();


	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

}

void VulkanApplication::createIndexBuffer()
{
	indexCount = static_cast<uint32_t>(indices.size());
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

}


void VulkanApplication::cleanup()
{
}

