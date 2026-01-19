#include "VkApplication.h"
#include "EngineWindow.h"
#include "../objects/vertex.h"
#include <stdexcept>	
#include <array>
#include "ShaderCompiler.h"
#include <iostream>
#include "../pipeline/computePipeline.h"

// Example vertices (triangle)
const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
	{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

const std::vector<uint32_t> indices = {
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
	, textureManager(nullptr)
	, bufferManager(nullptr)
	, depthImage(VK_NULL_HANDLE)
	, depthImageMemory(VK_NULL_HANDLE)
	, depthImageView(VK_NULL_HANDLE)
	, textureImage(VK_NULL_HANDLE)
	, textureImageMemory(VK_NULL_HANDLE)
	, textureImageView(VK_NULL_HANDLE)
	, textureSampler(VK_NULL_HANDLE)
	, computePipeline(nullptr)
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
	std::cout << "\n=== VERTEX LAYOUT DIAGNOSTICS ===" << std::endl;
	Vertex::printLayout();
	std::cout << "glm::vec3 size: " << sizeof(glm::vec3) << " bytes" << std::endl;
	std::cout << "glm::vec2 size: " << sizeof(glm::vec2) << " bytes" << std::endl;

	// Sample vertex to verify actual memory layout
	Vertex testVertex = { {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, {7.0f, 8.0f} };
	std::cout << "Test vertex data:" << std::endl;
	float* data = reinterpret_cast<float*>(&testVertex);
	for (int i = 0; i < sizeof(Vertex) / sizeof(float); i++) {
		std::cout << "  [" << i << "] = " << data[i] << std::endl;
	}
	std::cout << "================================\n" << std::endl;
	// 0. Ensure shaders are compiled to SPIR-V (will skip if up-to-date)
	ShaderCompiler::compileShadersIfNeeded();

	// 1. Create window
	window = new EngineWindow();
	window->init(800, 600, "Mukki Games Engine");
	glfwSetWindowUserPointer(window->getGLFWwindow(), this);
	// 2. Create instance (Vulkan context)
	instance.createInstance();

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
	
	// 7. Create command buffers FIRST (required by BufferManager and TextureManager)
	commandBufferManager = new CommandBufferManager();
	commandBufferManager->init(device, MAX_FRAMES_IN_FLIGHT);

	// 8. Initialize BufferManager and TextureManager (they depend on CommandBufferManager)
	bufferManager = new BufferManager();
	bufferManager->init(*device, *commandBufferManager);

	textureManager = new TextureManager();
	textureManager->init(*device, *commandBufferManager, *bufferManager);
	skybox = new SkyBox();
	skybox->init(device, textureManager, bufferManager,
		renderPass, ASSETS_PATH "studio_small.jpg",
		CubemapLayout::VerticalCross, MAX_FRAMES_IN_FLIGHT);
	// 9. Create depth resources using TextureManager
	VkExtent2D extent = swapChain->getSwapChainExtent();
	textureManager->createdepthResources(depthImage, depthImageMemory, depthImageView,
		extent.width, extent.height);
	objectLoader = new ObjectLoader();
	objectLoader->init(device, textureManager, bufferManager);

	// 10. Create framebuffers (attachments for render pass)
	swapChain->createFramebuffers(renderPass, depthImageView);

	// 11. Create descriptor set layout and pipeline layout BEFORE creating the pipeline
	createDescriptorSetLayout();
	createPipelineLayout();

	// 12. Create graphics pipeline (shaders and rendering configuration)
	createGraphicsPipeline();
	
	initComputePipeline();
	// 13. Create vertex and index buffers
	createVertexBuffer();
	createIndexBuffer();
	createTextureResources();
	createUniformBuffers();
	// 14. Create descriptor sets (for uniforms, textures, etc.)
	descriptorBoss = new VkDescriptorBoss(device, MAX_FRAMES_IN_FLIGHT);
	descriptorBoss->createDescriptorPool(MAX_FRAMES_IN_FLIGHT);
	descriptorBoss->createDescriptorSets(
		descriptorSetLayout,  // Use the member variable
		MAX_FRAMES_IN_FLIGHT,
		descriptorSets
	);

	descriptorBoss->updateDescriptorSets(
		descriptorSets,
		uniformBuffers,
		textureImageView,
		textureSampler
	);
	loadModel(ASSETS_PATH "Car_Model/scene.gltf");
	// 15. Create synchronization objects (semaphores and fences)
	createSyncObjects();
	
	// Initialize camera
	camera = new Camera(glm::vec3(0.0f, 0.0f, 3.0f));
	
	// Setup mouse callback
	glfwSetInputMode(window->getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window->getGLFWwindow(), mouseCallback);
	SetupUIManager();
}

void VulkanApplication::createSyncObjects()
{
	// Get the number of swapchain images
	size_t imageCount = swapChain->getSwapChainImages().size();
	
	// Per-frame semaphores and fences
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	
	// Per-swapchain-image semaphores
	renderFinishedSemaphores.resize(imageCount);
	imagesInFlight.resize(imageCount, VK_NULL_HANDLE);
	
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	
	// Create per-frame synchronization objects
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(device->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(device->getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create per-frame synchronization objects!");
		}
	}
	
	// Create per-swapchain-image semaphores
	for (size_t i = 0; i < imageCount; i++) {
		if (vkCreateSemaphore(device->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create per-image synchronization objects!");
		}
	}
}

void VulkanApplication::createVertexBuffer()
{
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
	std::cout << "\n=== VERTEX BUFFER CREATION ===" << std::endl;
	std::cout << "Vertex size: " << sizeof(Vertex) << " bytes" << std::endl;
	std::cout << "Vertex count: " << vertices.size() << std::endl;
	std::cout << "Total buffer size: " << bufferSize << " bytes" << std::endl;

	// Print actual vertex data being uploaded
	std::cout << "Vertex data:" << std::endl;
	for (size_t i = 0; i < vertices.size(); i++) {
		std::cout << "  Vertex[" << i << "]: pos("
			<< vertices[i].pos.x << ", "
			<< vertices[i].pos.y << ", "
			<< vertices[i].pos.z << "), color("
			<< vertices[i].color.r << ", "
			<< vertices[i].color.g << ", "
			<< vertices[i].color.b << ")" << std::endl;
	}
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

void VulkanApplication::createUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
	for(size_t i =0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		device->createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			uniformBuffers[i],
			uniformBuffersMemory[i]
		);
		vkMapMemory(device->getDevice(), uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
	}

}

void VulkanApplication::updateUniformBuffer(uint32_t currentImage)
{
	UniformBufferObject ubo{};
	// Build model matrix from transform
	glm::mat4 model = glm::mat4(1.0f);

	// Apply translation
	model = glm::translate(model, modelTransform.position);

	// Apply rotation (convert degrees to radians)
	model = glm::rotate(model, glm::radians(modelTransform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, glm::radians(modelTransform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::rotate(model, glm::radians(modelTransform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	// Apply uniform scale
	model = glm::scale(model, glm::vec3(modelTransform.scale));

	ubo.model = model;
	ubo.view = camera->getViewMatrix();
	VkExtent2D extent = swapChain->getSwapChainExtent();
	float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
	ubo.proj = camera->getProjectionMatrix(aspect);
	memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}
void VulkanApplication::createTextureResources()
{
	textureManager->createDebugTextureImage(textureImage, textureImageMemory, textureImageView);
	textureManager->createTextureSampler(textureSampler);
}

void VulkanApplication::drawFrame()
{
	// 1. Wait for the current frame's fence
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
	
	// 3. Check if a previous frame is using this image (wait for it)
	if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
		vkWaitForFences(device->getDevice(), 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
	}
	// Mark the image as being in use by this frame
	imagesInFlight[imageIndex] = inFlightFences[currentFrame];
	
	// 4. Reset fence only after we're sure we'll submit work
	vkResetFences(device->getDevice(), 1, &inFlightFences[currentFrame]);
	updateUniformBuffer(currentFrame);
	if (skybox) {
		VkExtent2D extent = swapChain->getSwapChainExtent();
		float aspect = extent.width / (float)extent.height;
		glm::mat4 view = camera->getSkyboxVPMatrix(aspect, 0.1f, 100.f);
		skybox->updateUniformBuffer(currentFrame, view);
	}
	// 5. Record command buffer
	commandBufferManager->resetCommandBuffer(currentFrame);
	VkCommandBuffer commandBuffer = commandBufferManager->getCommandBuffer(currentFrame);
	if (currentRenderMode == RenderMode::COMPUTE) {
		recordComputeCommandBuffer(commandBuffer, imageIndex);
	}
	else {
		if (modelLoaded && !modelDescriptorSets.empty()) {
			// Render loaded model with per-material textures
			commandBufferManager->recordModelCommandBuffer(
				commandBuffer,
				imageIndex,
				renderPass,
				swapChain->getSwapChainFramebuffers()[imageIndex],
				swapChain->getSwapChainExtent(),
				graphicsPipeline->getGraphicsPipeline(),
				additivePipeline ? additivePipeline->getGraphicsPipeline() : VK_NULL_HANDLE,
				pipelineLayout,  // Changed from graphicsPipeline->getPipelineLayout()
				loadedModel,
				skybox,
				modelDescriptorSets,
				currentFrame,
				*uiManager
			);
		}
		else {
			// Fallback to default quad rendering
			commandBufferManager->recordCommandBuffer(
				commandBuffer,
				imageIndex,
				renderPass,
				swapChain->getSwapChainFramebuffers()[imageIndex],
				swapChain->getSwapChainExtent(),
				graphicsPipeline->getGraphicsPipeline(),
				pipelineLayout,
				vertexBuffer,
				indexBuffer,
				descriptorSets,
				currentFrame,
				indexCount,
				*uiManager
			);
		}
	}
	
	
	// 6. Submit command buffer
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	
	VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	
	// Use the per-image semaphore for signaling
	VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[imageIndex]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	
	if (vkQueueSubmit(device->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}
	
	// 7. Present result
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
	
	// Cleanup old depth resources using TextureManager
	if (depthImageView != VK_NULL_HANDLE) {
		textureManager->destroyImageView(depthImageView);
		depthImageView = VK_NULL_HANDLE;
	}
	if (depthImage != VK_NULL_HANDLE) {
		textureManager->destroyImage(depthImage, depthImageMemory);
		depthImage = VK_NULL_HANDLE;
		depthImageMemory = VK_NULL_HANDLE;
	}
	
	// Cleanup old compute output image
	if (computeOutputImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(device->getDevice(), computeOutputImageView, nullptr);
		computeOutputImageView = VK_NULL_HANDLE;
	}
	if (computeOutputImage != VK_NULL_HANDLE) {
		vkDestroyImage(device->getDevice(), computeOutputImage, nullptr);
		computeOutputImage = VK_NULL_HANDLE;
	}
	if (computeOutputImageMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device->getDevice(), computeOutputImageMemory, nullptr);
		computeOutputImageMemory = VK_NULL_HANDLE;
	}
	
	// Cleanup old per-image semaphores
	for (size_t i = 0; i < renderFinishedSemaphores.size(); i++) {
		vkDestroySemaphore(device->getDevice(), renderFinishedSemaphores[i], nullptr);
	}
	
	// Cleanup old graphics pipeline
	if (graphicsPipeline) {
		delete graphicsPipeline;
		graphicsPipeline = nullptr;
	}
	if(additivePipeline) {
		delete additivePipeline;
		additivePipeline = nullptr;
	}
	
	// Cleanup old swap chain
	swapChain->cleanup();
	
	// Recreate swap chain and dependent objects
	swapChain->initSwap(*device, window->getSurface(), window->getGLFWwindow());
	swapChain->createImageViews();
	
	VkExtent2D swapExtent = swapChain->getSwapChainExtent();
	textureManager->createdepthResources(depthImage, depthImageMemory, depthImageView, swapExtent.width, swapExtent.height);
	swapChain->createFramebuffers(renderPass, depthImageView);
	
	// Recreate graphics pipeline
	createGraphicsPipeline();
	
	// Recreate compute output image
	createComputeOutputImage();
	
	// IMPORTANT: Update compute pipeline descriptor sets with new image view
	if (computePipeline) {
		computePipeline->resetDesciriptorPool(device);
		computePipeline->createDescriptorSets(device, computeOutputImageView);
	}
	
	// Recreate per-image semaphores for new swapchain
	size_t imageCount = swapChain->getSwapChainImages().size();
	renderFinishedSemaphores.resize(imageCount);
	imagesInFlight.resize(imageCount, VK_NULL_HANDLE);
	
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	
	for (size_t i = 0; i < imageCount; i++) {
		if (vkCreateSemaphore(device->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to recreate per-image semaphores!");
		}
	}
}

void VulkanApplication::SetupUIManager()
{
	uiManager = new UIManager();
	UIRenderData renderData{};
	renderData.instance = instance.getInstance();
	renderData.physicalDevice = device->getPhysicalDevice();
	renderData.device = device->getDevice();
	renderData.queueFamily = device->findQueueFamilies(device->getPhysicalDevice()).graphicsFamily.value();
	renderData.queue = device->getGraphicsQueue();
	renderData.renderPass = renderPass;
	renderData.commandPool = commandBufferManager->getCommandPool();
	renderData.descriptorPool = descriptorBoss->getDescriptorPool();
	renderData.imageCount = static_cast<uint32_t>(swapChain->getSwapChainImages().size());
	renderData.minImageCount = 2; // Typical minimum
	uiManager->init(renderData, window);
}

void VulkanApplication::mainLoop()
{
	while (!glfwWindowShouldClose(window->getGLFWwindow())) {
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		
		glfwPollEvents();
		processInput();

		uiManager->newFrame();
		float fps = 1.0f / deltaTime;
		uiManager->renderDebugWindow(fps, deltaTime);
		uiManager->renderCameraInfo(camera->position, camera->front);
		uiManager->renderModelTransformWindow(modelTransform, deltaTime);
		drawFrame();
	}
	
	vkDeviceWaitIdle(device->getDevice());
}

void VulkanApplication::cleanup()
{
	// Cleanup in reverse order of creation
	cleanupComputeResources();
	//cleanup uniform buffers
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (uniformBuffers[i] != VK_NULL_HANDLE) {
			vkDestroyBuffer(device->getDevice(), uniformBuffers[i], nullptr);
		}
		if (uniformBuffersMemory[i] != VK_NULL_HANDLE) {
			vkFreeMemory(device->getDevice(), uniformBuffersMemory[i], nullptr);
		}
	}
	// Cleanup per-frame synchronization objects
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device->getDevice(), imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(device->getDevice(), inFlightFences[i], nullptr);
	}
	
	// Cleanup per-image synchronization objects
	for (size_t i = 0; i < renderFinishedSemaphores.size(); i++) {
		vkDestroySemaphore(device->getDevice(), renderFinishedSemaphores[i], nullptr);
	}
	
	// Cleanup texture resources
	if (textureSampler != VK_NULL_HANDLE) {
		textureManager->destroySampler(textureSampler);
	}
	if (textureImageView != VK_NULL_HANDLE) {
		textureManager->destroyImageView(textureImageView);
	}
	if (textureImage != VK_NULL_HANDLE) {
		textureManager->destroyImage(textureImage, textureImageMemory);
	}
	
	// Cleanup depth resources
	if (textureManager) {
		if (depthImageView != VK_NULL_HANDLE) {
			textureManager->destroyImageView(depthImageView);
		}
		if (depthImage != VK_NULL_HANDLE) {
			textureManager->destroyImage(depthImage, depthImageMemory);
		}
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
	if(skybox) {
		skybox->cleanup();
		delete skybox;
	}
	if (graphicsPipeline) {
		delete graphicsPipeline;
	}
	if(additivePipeline) {
		delete additivePipeline;
	}
	if(pipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(device->getDevice(), pipelineLayout, nullptr);
	}
	if(descriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(device->getDevice(), descriptorSetLayout, nullptr);
	}
	if (swapChain) {
		swapChain->cleanup();
		delete swapChain;
	}
	
	if (renderPassObj) {
		delete renderPassObj;
	}
	
	if (textureManager) {
		delete textureManager;
	}
	
	if (bufferManager) {
		delete bufferManager;
	}
	if(uiManager) {
		uiManager->cleanup();
		delete uiManager;
	}
	if(camera) {
		delete camera;
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

void VulkanApplication::toggleCursor()
{
	cursorEnabled = !cursorEnabled;
	if(cursorEnabled) {
		glfwSetInputMode(window->getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	} else {
		glfwSetInputMode(window->getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		firstMouse = true; // Reset mouse tracking
	}
}

void VulkanApplication::processInput() {
	GLFWwindow* win = window->getGLFWwindow();
	static bool renderKeyPressed = false;
	static bool cursorKeyPressed = false;
	if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(win, true);
	if (glfwGetKey(win, GLFW_KEY_TAB) == GLFW_PRESS) {
		if (!cursorKeyPressed) {
			toggleCursor();
			cursorKeyPressed = true;
		}
	}
	else {
		cursorKeyPressed = false;
	}
	if (!cursorEnabled) {
		if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)
			camera->processKeyboardInput(FORWARD, deltaTime);
		if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
			camera->processKeyboardInput(BACKWARD, deltaTime);
		if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS)
			camera->processKeyboardInput(LEFT, deltaTime);
		if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS)
			camera->processKeyboardInput(RIGHT, deltaTime);
		if (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS)
			camera->processKeyboardInput(UP, deltaTime);
		if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
			camera->processKeyboardInput(DOWN, deltaTime);
	}
	if (glfwGetKey(win, GLFW_KEY_B) == GLFW_PRESS) {
		if(!renderKeyPressed) {
			toggleRenderMode();
			renderKeyPressed = true;
		}

	}
	else {
		renderKeyPressed = false;
	}
}

void VulkanApplication::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
	auto app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
	if(app->cursorEnabled) {
		return; // Ignore mouse movement when cursor is enabled
	}
	if (app->firstMouse) {
		app->lastX = static_cast<float>(xpos);
		app->lastY = static_cast<float>(ypos);
		app->firstMouse = false;
	}
	
	float xoffset = static_cast<float>(xpos) - app->lastX;
	float yoffset = app->lastY - static_cast<float>(ypos); // Reversed: y-coordinates go from bottom to top
	
	app->lastX = static_cast<float>(xpos);
	app->lastY = static_cast<float>(ypos);
	
	app->camera->processMouseMovement(xoffset, yoffset);
}

void VulkanApplication::createModelDescriptorSets()
{
	if (!modelLoaded || loadedModel.materials.empty()) {
		return;
	}

	size_t materialCount = loadedModel.materials.size();
	modelDescriptorSets.resize(materialCount);

	for (size_t matIndex = 0; matIndex < materialCount; matIndex++) {
		const Material& material = loadedModel.materials[matIndex];

		// Allocate descriptor sets for this material (one per frame in flight)
		modelDescriptorSets[matIndex].resize(MAX_FRAMES_IN_FLIGHT);

		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorBoss->getDescriptorPool();
		allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
		allocInfo.pSetLayouts = layouts.data();

		if (vkAllocateDescriptorSets(device->getDevice(), &allocInfo, modelDescriptorSets[matIndex].data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate model descriptor sets!");
		}

		// Update descriptor sets with the material's texture
		for (size_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
			std::vector<VkWriteDescriptorSet> descriptorWrites;

			// UBO (binding 0)
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = uniformBuffers[frame];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniformBufferObject);

			VkWriteDescriptorSet uboWrite{};
			uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			uboWrite.dstSet = modelDescriptorSets[matIndex][frame];
			uboWrite.dstBinding = 0;
			uboWrite.dstArrayElement = 0;
			uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			uboWrite.descriptorCount = 1;
			uboWrite.pBufferInfo = &bufferInfo;
			descriptorWrites.push_back(uboWrite);

			// Texture sampler (binding 1)
			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			// Get the texture for this material
			if (material.baseColorTextureIndex >= 0 &&
				material.baseColorTextureIndex < static_cast<int32_t>(loadedModel.textures.size())) {
				const LoadedTexture& tex = loadedModel.textures[material.baseColorTextureIndex];
				imageInfo.imageView = tex.imageView;
				imageInfo.sampler = tex.sampler;
			}
			else {
				// Use default texture if material has no texture
				imageInfo.imageView = textureImageView;
				imageInfo.sampler = textureSampler;
			}

			VkWriteDescriptorSet samplerWrite{};
			samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			samplerWrite.dstSet = modelDescriptorSets[matIndex][frame];
			samplerWrite.dstBinding = 1;
			samplerWrite.dstArrayElement = 0;
			samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			samplerWrite.descriptorCount = 1;
			samplerWrite.pImageInfo = &imageInfo;
			descriptorWrites.push_back(samplerWrite);

			vkUpdateDescriptorSets(device->getDevice(),
				static_cast<uint32_t>(descriptorWrites.size()),
				descriptorWrites.data(), 0, nullptr);
		}

		std::cout << "  Created descriptor sets for material " << matIndex;
		if (material.baseColorTextureIndex >= 0) {
			std::cout << " (texture " << material.baseColorTextureIndex << ")";
		}
		std::cout << std::endl;
	}
}

void VulkanApplication::initComputePipeline() {
	computePipeline = new ComputePipeline();
	
	// Create the output storage image for compute shader
	createComputeOutputImage();
	
	// Initialize compute pipeline components
	computePipeline->createDescriptorSetLayout(device);
	computePipeline->createDescriptorPool(device, MAX_FRAMES_IN_FLIGHT);
	computePipeline->createDescriptorSets(device, computeOutputImageView);
	computePipeline->createComputePipeline(device, "comp.spv");

}
void VulkanApplication::createComputeOutputImage() {
	VkExtent2D extent = swapChain->getSwapChainExtent();
	computeOutputImageExtent = extent;
	VkFormat storageFormat = VK_FORMAT_R8G8B8A8_UNORM;
	// Create storage image
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = extent.width;
	imageInfo.extent.height = extent.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = storageFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(device->getDevice(), &imageInfo, nullptr, &computeOutputImage) != VK_SUCCESS) {
		throw std::runtime_error("failed to create compute output image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device->getDevice(), computeOutputImage, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &computeOutputImageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate compute output image memory!");
	}

	vkBindImageMemory(device->getDevice(), computeOutputImage, computeOutputImageMemory, 0);

	// Create image view
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = computeOutputImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = storageFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(device->getDevice(), &viewInfo, nullptr, &computeOutputImageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create compute output image view!");
	}

	// Transition image layout to GENERAL for compute shader access
	VkCommandBuffer commandBuffer = commandBufferManager->beginSingleTimeCommands();
	
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = computeOutputImage;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
	
	commandBufferManager->endSingleTimeCommands(commandBuffer);
}

void VulkanApplication::recordComputeCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording compute command buffer!");
	}

	// Bind compute pipeline
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->getPipeline());
	
	// Bind descriptor sets
	VkDescriptorSet descSet = computePipeline->getDescriptorSet();
	vkCmdBindDescriptorSets(
		commandBuffer, 
		VK_PIPELINE_BIND_POINT_COMPUTE,
		computePipeline->getPipelineLayout(), 
		0, 1, 
		&descSet, 
		0, nullptr
	);

	// Dispatch compute work
	VkExtent2D extent = swapChain->getSwapChainExtent();
	ComputePushConstants pushConstants{};
	pushConstants.iResolution[0] = static_cast<float>(extent.width);
	pushConstants.iResolution[1] = static_cast<float>(extent.height);
	pushConstants.iTime = static_cast<float>(glfwGetTime());
	vkCmdPushConstants(
		commandBuffer,
		computePipeline->getPipelineLayout(),
		VK_SHADER_STAGE_COMPUTE_BIT,
		0,
		sizeof(ComputePushConstants),
		&pushConstants
	);
	uint32_t groupCountX = (extent.width + 15) / 16;  // 16x16 work groups
	uint32_t groupCountY = (extent.height + 15) / 16;
	vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

	// Transition compute output for transfer
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = computeOutputImage;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	// Transition swapchain image for transfer destination
	VkImageMemoryBarrier swapBarrier{};
	swapBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	swapBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	swapBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	swapBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	swapBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	swapBarrier.image = swapChain->getSwapChainImages()[imageIndex];
	swapBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	swapBarrier.subresourceRange.baseMipLevel = 0;
	swapBarrier.subresourceRange.levelCount = 1;
	swapBarrier.subresourceRange.baseArrayLayer = 0;
	swapBarrier.subresourceRange.layerCount = 1;
	swapBarrier.srcAccessMask = 0;
	swapBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &swapBarrier
	);

	// Copy compute output to swapchain image
	VkImageCopy copyRegion{};
	copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.srcSubresource.layerCount = 1;
	copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.dstSubresource.layerCount = 1;
	copyRegion.extent.width = computeOutputImageExtent.width;
	copyRegion.extent.height = computeOutputImageExtent.height;
	copyRegion.extent.depth = 1;

	vkCmdCopyImage(
		commandBuffer,
		computeOutputImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		swapChain->getSwapChainImages()[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &copyRegion
	);

	// Transition swapchain image for presentation
	swapBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	swapBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	swapBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	swapBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &swapBarrier
	);

	// Transition compute output back to GENERAL
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = swapChain->getSwapChainFramebuffers()[imageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapChain->getSwapChainExtent();
	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	clearValues[1].depthStencil = { 1.0f, 0 };
	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	uiManager->render(commandBuffer);
	/*vkCmdEndRenderPass(commandBuffer);*/
	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record compute command buffer!");
	}
}

void VulkanApplication::loadModel(const std::string& filepath)
{
	if (objectLoader->loadGLTF(filepath, loadedModel)) {
		objectLoader->createModelBuffers(loadedModel);
		modelLoaded = true;
		indexCount = static_cast<uint32_t>(loadedModel.indices.size());
		if(!loadedModel.textures.empty() && loadedModel.textures[0].imageView != VK_NULL_HANDLE) {
			// create descriptor sets for the loaded model
			createModelDescriptorSets();
		}
		std::cout << "Model loaded successfully: " << filepath << std::endl;
	}
	else {
		std::cerr << "Failed to load model: " << filepath << std::endl;
	}
}

void VulkanApplication::toggleRenderMode()
{
    if (currentRenderMode == RenderMode::GRAPHICS) {
        currentRenderMode = RenderMode::COMPUTE;
        std::cout << "Switched to Compute rendering mode" << std::endl;
    } else {
        currentRenderMode = RenderMode::GRAPHICS;
        std::cout << "Switched to Graphics rendering mode" << std::endl;
    }
}

void VulkanApplication::cleanupComputeResources()
{
    if (computePipeline) {
        computePipeline->cleanup(device);
        delete computePipeline;
        computePipeline = nullptr;
    }
    if (computeOutputImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device->getDevice(), computeOutputImageView, nullptr);
    }
    if (computeOutputImage != VK_NULL_HANDLE) {
        vkDestroyImage(device->getDevice(), computeOutputImage, nullptr);
    }
    if (computeOutputImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device->getDevice(), computeOutputImageMemory, nullptr);
    }
	if(objectLoader) {
		objectLoader->destroyModel(loadedModel);
		delete objectLoader;
	}
}

void VulkanApplication::createDescriptorSetLayout()
{
	// UBO binding (binding = 0)
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	// Sampler binding (binding = 1)
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = {
		uboLayoutBinding,
		samplerLayoutBinding
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(device->getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void VulkanApplication::createPipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(device->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}
}

void VulkanApplication::createGraphicsPipeline()
{
	PipelineConfigInfo pipelineConfig{};
	VulkanPipeline::defaultPipelineConfigInfo(pipelineConfig);
	pipelineConfig.renderPass = renderPass;
	pipelineConfig.pipelineLayout = pipelineLayout;
	VulkanPipeline::enableAlphaBlending(pipelineConfig);
	graphicsPipeline = new VulkanPipeline(
		device,
		"vert.spv",
		"frag.spv",
		pipelineConfig
	);
	PipelineConfigInfo additiveConfig{};
	VulkanPipeline::defaultPipelineConfigInfo(additiveConfig);
	additiveConfig.renderPass = renderPass;
	additiveConfig.pipelineLayout = pipelineLayout;
	VulkanPipeline::enableAdditiveBlending(additiveConfig);
	additivePipeline = new VulkanPipeline(
		device,
		"vert.spv",
		"frag.spv",
		additiveConfig
	);
}