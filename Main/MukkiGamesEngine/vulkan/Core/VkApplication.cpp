#include "VkApplication.h"
#include "EngineWindow.h"
#include "../objects/vertex.h"
#include <stdexcept>	
#include <array>

// Example vertices (triangle)
const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
	{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0
};

VulkanApplication::VulkanApplication()
	: window(nullptr)
	, device(nullptr)
	, swapChain(nullptr)
	, renderPassObj(nullptr)
	, renderPass(VK_NULL_HANDLE)
	, graphicsPipeline(nullptr)
	, commandBufferManager(nullptr)
	, currentFrame(0)
	, vertexBuffer(VK_NULL_HANDLE)
	, vertexBufferMemory(VK_NULL_HANDLE)
	, indexBuffer(VK_NULL_HANDLE)
	, indexBufferMemory(VK_NULL_HANDLE)
	, indexCount(0)
	, descriptorBoss(nullptr)
{
}

VulkanApplication::~VulkanApplication()
{
}

void VulkanApplication::run()
{
	initVulkan();
	mainLoop();
	cleanup();
}

void VulkanApplication::initVulkan()
{
	// 1. Create instance (Vulkan context)
	instance.createInstance();
	
	// 2. Create window
	window = new EngineWindow();
	window->init(800, 600, "Mukki Games Engine");
	
	// 3. Create surface (connection between Vulkan and window)
	VkSurfaceKHR surface = window->createSurface(instance.getInstance());
	
	// 4. Create device (select GPU and create logical device)
	device = new Device(instance, surface);
	
	// 5. Create swap chain (manages images for presentation)
	swapChain = new VulkanSwap();
	swapChain->initSwap(*device, surface, window->getGLFWwindow());
	swapChain->createImageViews();
	
	// 6. Create render pass (defines how rendering operations are performed)
	renderPassObj = new VulkanRenderPass(device, swapChain->getSwapChainImageFormat());
	renderPass = renderPassObj->getRenderPass();
	
	// 7. Create framebuffers (attachments for render pass)
	swapChain->createFramebuffers(renderPass);
	
	// 8. Create graphics pipeline (shaders and rendering configuration)
	graphicsPipeline = new VulkanPipeline(
		device, 
		renderPass, 
		"shaders/vert.spv", 
		"shaders/frag.spv", 
		window, 
		&instance
	);
	
	// 9. Create command buffers (record drawing commands)
	commandBufferManager = new CommandBufferManager();
	commandBufferManager->init(device, MAX_FRAMES_IN_FLIGHT);
	
	// 10. Create vertex and index buffers
	createVertexBuffer();
	createIndexBuffer();
	
	// 11. Create descriptor sets (for uniforms, textures, etc.)
	descriptorBoss = new VkDescriptorBoss(device, MAX_FRAMES_IN_FLIGHT);
	descriptorBoss->createDescriptorPool(MAX_FRAMES_IN_FLIGHT);
	
	// 12. Create synchronization objects (semaphores and fences)
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
	
	// Create staging buffer (CPU accessible)
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	device->createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);
	
	// Copy vertex data to staging buffer
	void* data;
	vkMapMemory(device->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vkUnmapMemory(device->getDevice(), stagingBufferMemory);
	
	// Create vertex buffer (GPU local)
	device->createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBuffer,
		vertexBufferMemory
	);
	
	// Copy from staging to vertex buffer
	device->copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
	
	// Cleanup staging buffer
	vkDestroyBuffer(device->getDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device->getDevice(), stagingBufferMemory, nullptr);
}

void VulkanApplication::createIndexBuffer()
{
	indexCount = static_cast<uint32_t>(indices.size());
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
	
	// Create staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	device->createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);
	
	// Copy index data
	void* data;
	vkMapMemory(device->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(device->getDevice(), stagingBufferMemory);
	
	// Create index buffer
	device->createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBuffer,
		indexBufferMemory
	);
	
	// Copy from staging to index buffer
	device->copyBuffer(stagingBuffer, indexBuffer, bufferSize);
	
	// Cleanup
	vkDestroyBuffer(device->getDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device->getDevice(), stagingBufferMemory, nullptr);
}

void VulkanApplication::drawFrame()
{
	// 1. Wait for previous frame to finish
	vkWaitForFences(device->getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
	
	// 2. Acquire image from swap chain
	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(
		device->getDevice(), 
		swapChain->getSwapChain(), 
		UINT64_MAX,
		imageAvailableSemaphores[currentFrame], 
		VK_NULL_HANDLE, 
		&imageIndex
	);
	
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapChain();
		return;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}
	
	// 3. Reset fence
	vkResetFences(device->getDevice(), 1, &inFlightFences[currentFrame]);
	
	// 4. Record command buffer
	descriptorBoss->createDescriptorSets(
		graphicsPipeline->getDescriptorSetLayout(),
		MAX_FRAMES_IN_FLIGHT,
		descriptorSets
	);
	commandBufferManager->resetCommandBuffer(currentFrame);
	VkCommandBuffer commandBuffer = commandBufferManager->getCommandBuffer(currentFrame);
	commandBufferManager->recordCommandBuffer(
		commandBuffer,
		imageIndex,
		renderPass,
		swapChain->getSwapChainFramebuffers()[imageIndex],
		swapChain->getSwapChainExtent(),
		graphicsPipeline->getGraphicsPipeline(),
		graphicsPipeline->getPipelineLayout(),
		vertexBuffer,
		indexBuffer,
		descriptorSets,
		currentFrame,
		indexCount
	);
	
	// 5. Submit command buffer
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	
	VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	
	VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	
	if (vkQueueSubmit(device->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}
	
	// 6. Present result
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	
	VkSwapchainKHR swapChains[] = {swapChain->getSwapChain()};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	
	result = vkQueuePresentKHR(device->getPresentQueue(), &presentInfo);
	
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		recreateSwapChain();
	} else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}
	
	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanApplication::recreateSwapChain()
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(window->getGLFWwindow(), &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window->getGLFWwindow(), &width, &height);
		glfwWaitEvents();
	}
	
	vkDeviceWaitIdle(device->getDevice());
	
	// Cleanup old swap chain
	swapChain->cleanup();
	
	// Recreate swap chain and dependent objects
	swapChain->initSwap(*device, window->getSurface(), window->getGLFWwindow());
	swapChain->createImageViews();
	swapChain->createFramebuffers(renderPass);
}

void VulkanApplication::mainLoop()
{
	while (!glfwWindowShouldClose(window->getGLFWwindow())) {
		glfwPollEvents();
		drawFrame();
	}
	
	vkDeviceWaitIdle(device->getDevice());
}

void VulkanApplication::cleanup()
{
	// Cleanup in reverse order of creation
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device->getDevice(), renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(device->getDevice(), imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(device->getDevice(), inFlightFences[i], nullptr);
	}
	
	
	vkDestroyBuffer(device->getDevice(), indexBuffer, nullptr);
	vkFreeMemory(device->getDevice(), indexBufferMemory, nullptr);
	vkDestroyBuffer(device->getDevice(), vertexBuffer, nullptr);
	vkFreeMemory(device->getDevice(), vertexBufferMemory, nullptr);
	
	if (descriptorBoss) {
		descriptorBoss->cleanup();
		delete descriptorBoss;
	}
	
	if (commandBufferManager) {
		commandBufferManager->cleanup();
		delete commandBufferManager;
	}
	
	if (graphicsPipeline) {
		delete graphicsPipeline;
	}
	
	if (swapChain) {
		swapChain->cleanup();
		delete swapChain;
	}
	
	if (renderPassObj) {
		delete renderPassObj;
	}
	
	if (device) {
		device->cleanup();
		delete device;
	}
	
	if (window) {
		window->cleanup();
		delete window;
	}
	
	instance.cleanup();
}

