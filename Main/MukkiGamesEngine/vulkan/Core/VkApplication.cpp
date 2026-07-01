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
		{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
		{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
		{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
	};

const std::vector<uint32_t> indices = {
	0, 1, 2, 2, 3, 0
};

VulkanApplication::VulkanApplication()
	: renderPass(VK_NULL_HANDLE)
	, currentFrame(0)
	, vertexBuffer(VK_NULL_HANDLE)
	, vertexBufferMemory(VK_NULL_HANDLE)
	, indexBuffer(VK_NULL_HANDLE)
	, indexBufferMemory(VK_NULL_HANDLE)
	, indexCount(0)
	, depthImage(VK_NULL_HANDLE)
	, depthImageMemory(VK_NULL_HANDLE)
	, depthImageView(VK_NULL_HANDLE)
	, textureImage(VK_NULL_HANDLE)
	, textureImageMemory(VK_NULL_HANDLE)
	, textureImageView(VK_NULL_HANDLE)
	, textureSampler(VK_NULL_HANDLE)
{
}

void VulkanApplication::createRayTracingUniformBuffer()
{
    VkDeviceSize bufferSize = sizeof(glm::mat4) * 5 + sizeof(glm::vec4) + sizeof(GPULight) * MAX_LIGHTS + sizeof(glm::vec4);
	bufferManager->createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		rayTracingUniformBuffer,
		rayTracingUniformBufferMemory);

	vkMapMemory(device->getDevice(), rayTracingUniformBufferMemory, 0, bufferSize, 0, &rayTracingUniformBufferMapped);
}

void VulkanApplication::createRayTracingGeometryBuffers()
{
	 cleanupRayTracingGeometryBuffers();

	if (loadedModel.meshes.empty()) {
		return;
	}

	std::vector<RayTracingPrimitiveInfo> primitiveInfos;
	primitiveInfos.reserve(loadedModel.indices.size() / 3);
	std::vector<RayTracingMeshInfo> meshInfos;
	meshInfos.reserve(loadedModel.meshes.size());

	uint32_t primitiveOffset = 0;
	for (const auto& mesh : loadedModel.meshes) {
		RayTracingMeshInfo meshInfo{};
		meshInfo.primitiveOffset = primitiveOffset;
		meshInfo.primitiveCount = static_cast<uint32_t>(mesh.primitives.size());
		meshInfo.pad0 = 0;
		meshInfo.pad1 = 0;
		meshInfos.push_back(meshInfo);

		for (const auto& primitive : mesh.primitives) {
			RayTracingPrimitiveInfo primInfo{};
			primInfo.firstIndex = primitive.firstIndex;
			primInfo.indexCount = primitive.indexCount;
			int32_t texIdx = -1;
			float metallic = 0.0f;
			float roughness = 1.0f;
			glm::vec3 baseColor = glm::vec3(1.0f);
			if (primitive.materialIndex >= 0 &&
				primitive.materialIndex < static_cast<int32_t>(loadedModel.materials.size())) {
				const auto& mat = loadedModel.materials[primitive.materialIndex];
				texIdx = mat.baseColorTextureIndex;
				metallic = mat.metallicFactor;
				roughness = mat.roughnessFactor;
				baseColor = glm::vec3(mat.baseColorFactor);
			}
			primInfo.textureIndex = texIdx;
			primInfo.metallicFactor = metallic;
			primInfo.roughnessFactor = roughness;
			primInfo.baseColorR = baseColor.r;
			primInfo.baseColorG = baseColor.g;
			primInfo.baseColorB = baseColor.b;
			primitiveInfos.push_back(primInfo);
		}

		primitiveOffset += static_cast<uint32_t>(mesh.primitives.size());
	}

	if (!primitiveInfos.empty()) {
		VkDeviceSize primBufferSize = sizeof(RayTracingPrimitiveInfo) * primitiveInfos.size();
		device->createBuffer(
			primBufferSize,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			rayTracingPrimitiveBuffer,
			rayTracingPrimitiveBufferMemory);

		void* mapped = nullptr;
		vkMapMemory(device->getDevice(), rayTracingPrimitiveBufferMemory, 0, primBufferSize, 0, &mapped);
		memcpy(mapped, primitiveInfos.data(), primBufferSize);
		vkUnmapMemory(device->getDevice(), rayTracingPrimitiveBufferMemory);
	}

	if (!meshInfos.empty()) {
		VkDeviceSize meshBufferSize = sizeof(RayTracingMeshInfo) * meshInfos.size();
		device->createBuffer(
			meshBufferSize,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			rayTracingMeshBuffer,
			rayTracingMeshBufferMemory);

		void* mapped = nullptr;
		vkMapMemory(device->getDevice(), rayTracingMeshBufferMemory, 0, meshBufferSize, 0, &mapped);
		memcpy(mapped, meshInfos.data(), meshBufferSize);
		vkUnmapMemory(device->getDevice(), rayTracingMeshBufferMemory);
	}
}

void VulkanApplication::cleanupRayTracingGeometryBuffers()
{
	if (rayTracingPrimitiveBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device->getDevice(), rayTracingPrimitiveBuffer, nullptr);
		rayTracingPrimitiveBuffer = VK_NULL_HANDLE;
	}
	if (rayTracingPrimitiveBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device->getDevice(), rayTracingPrimitiveBufferMemory, nullptr);
		rayTracingPrimitiveBufferMemory = VK_NULL_HANDLE;
	}
	if (rayTracingMeshBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device->getDevice(), rayTracingMeshBuffer, nullptr);
		rayTracingMeshBuffer = VK_NULL_HANDLE;
	}
	if (rayTracingMeshBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device->getDevice(), rayTracingMeshBufferMemory, nullptr);
		rayTracingMeshBufferMemory = VK_NULL_HANDLE;
	}
}

VulkanApplication::~VulkanApplication()
{
}

void VulkanApplication::updateRayTracingUniformBuffer()
{
	VkExtent2D extent = swapChain->getSwapChainExtent();
	float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
	glm::mat4 view = camera->getViewMatrix();
	glm::mat4 proj = camera->getProjectionMatrix(aspect);
	glm::mat4 invView = glm::inverse(view);
	glm::mat4 invProj = glm::inverse(proj);
	glm::vec4 camPos = glm::vec4(camera->position, 1.0f);

	struct RayTracingCameraUBO {
		glm::mat4 invView;
		glm::mat4 invProj;
		glm::vec4 cameraPos;
		glm::mat4 model;
		glm::mat4 invModel;
		glm::mat4 normalMatrix;
        GPULight lights[MAX_LIGHTS];
		glm::vec4 lightParams;
	};

	RayTracingCameraUBO ubo{};
	ubo.invView = invView;
	ubo.invProj = invProj;
	ubo.cameraPos = camPos;

	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, modelTransform.position);
	model = glm::rotate(model, glm::radians(modelTransform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, glm::radians(modelTransform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::rotate(model, glm::radians(modelTransform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	model = glm::scale(model, glm::vec3(modelTransform.scale));
	ubo.model = model;
	ubo.invModel = glm::inverse(model);
	ubo.normalMatrix = glm::transpose(ubo.invModel);

	ubo.lightParams = glm::vec4(0.0f);
	ubo.lightParams.y = ambientStrength;
	ubo.lightParams.z = static_cast<float>(lights.size());
	ubo.lightParams.w = static_cast<float>(accumulationFrameCount);

	int lightCount = 0;
	for (size_t i = 0; i < lights.size() && lightCount < MAX_LIGHTS; ++i) {
		if (lights[i].enabled) {
			ubo.lights[lightCount] = lights[i].toGPU();
			++lightCount;
		}
	}
	ubo.lightParams.x = static_cast<float>(lightCount);

	memcpy(rayTracingUniformBufferMapped, &ubo, sizeof(ubo));
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
	Vertex testVertex = { {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, {7.0f, 8.0f}, {0.0f, 0.0f, 1.0f} };
	std::cout << "Test vertex data:" << std::endl;
	float* data = reinterpret_cast<float*>(&testVertex);
	for (int i = 0; i < sizeof(Vertex) / sizeof(float); i++) {
		std::cout << "  [" << i << "] = " << data[i] << std::endl;
	}
	std::cout << "================================\n" << std::endl;
	// 0. Ensure shaders are compiled to SPIR-V (will skip if up-to-date)
	ShaderCompiler::compileShadersIfNeeded();

	// 1. Create window
	window = std::make_unique<EngineWindow>();
	window->init(800, 600, "Mukki Games Engine");
	glfwSetWindowUserPointer(window->getGLFWwindow(), this);
	// 2. Create instance (Vulkan context)
	instance.createInstance();

	// 3. Create surface (connection between Vulkan and window)
	VkSurfaceKHR surface = window->createSurface(instance.getInstance());

	// 4. Create device (select GPU and create logical device)
	device = std::make_unique<Device>(instance, surface);

	// 5. Create swap chain (manages images for presentation)
	swapChain = std::make_unique<VulkanSwap>();
	swapChain->initSwap(*device, surface, window->getGLFWwindow());
	swapChain->createImageViews();

	// 6. Create render pass (defines how rendering operations are performed)
	renderPassObj = std::make_unique<VulkanRenderPass>(device.get(), swapChain->getSwapChainImageFormat());
	renderPass = renderPassObj->getRenderPass();

	// 7. Create command buffers FIRST (required by BufferManager and TextureManager)
	commandBufferManager = std::make_unique<CommandBufferManager>();
	commandBufferManager->init(device.get(), MAX_FRAMES_IN_FLIGHT);

	// 8. Initialize BufferManager and TextureManager (they depend on CommandBufferManager)
	bufferManager = std::make_unique<BufferManager>();
	bufferManager->init(*device, *commandBufferManager);

	textureManager = std::make_unique<TextureManager>();
	textureManager->init(*device, *commandBufferManager, *bufferManager);

	rayTracingAS = std::make_unique<RayTracingAS>();
	rayTracingAS->init(device.get(), commandBufferManager.get());

	objectLoader = std::make_unique<ObjectLoader>();
	objectLoader->init(device.get(), textureManager.get(), bufferManager.get());

	sceneLoader = std::make_unique<SceneLoader>();
	sceneLoader->init(device.get(), textureManager.get(), bufferManager.get(), objectLoader.get());
	sceneLoader->loadScene(ASSETS_PATH + availableScenes[0] );

	skybox = std::make_unique<SkyBox>();
	std::string skyboxFileName = sceneLoader->getConfig().skyboxPath;
	if(skyboxFileName.empty()) skyboxFileName = "studio_small.hdr";
	std::string fullSkyboxPath = std::string(ASSETS_PATH) + skyboxFileName;

	skybox->init(device.get(), textureManager.get(), bufferManager.get(),
		renderPass, fullSkyboxPath,
		CubemapLayout::VerticalCross, MAX_FRAMES_IN_FLIGHT);

	// 9. Create depth resources using TextureManager
	VkExtent2D extent = swapChain->getSwapChainExtent();
	textureManager->createdepthResources(depthImage, depthImageMemory, depthImageView,
		extent.width, extent.height);

	// 10. Create framebuffers (attachments for render pass)
	swapChain->createFramebuffers(renderPass, depthImageView);

	// 11. Create descriptor set layout and pipeline layout BEFORE creating the pipeline
	createDescriptorSetLayout();
    createRayTracingDescriptorSetLayout();
	createPipelineLayout();

	// 12. Create graphics pipeline (shaders and rendering configuration)
	createGraphicsPipeline();

	initComputePipeline();
	createAccumulationImage();
    initRayTracingPipeline();
	// 13. Create vertex and index buffers
	createVertexBuffer();
	createIndexBuffer();
	createTextureResources();
	createUniformBuffers();
	createDefaultMaterialUniformBuffers();
	createRayTracingUniformBuffer();

	lights = sceneLoader->getLights();
	ambientStrength = sceneLoader->getConfig().ambientStrenght;
	if(lights.empty()) {
		setupDefaultLights();
	}

	// 14. Create descriptor sets (for uniforms, textures, etc.)
	descriptorBoss = std::make_unique<VkDescriptorBoss>(device.get(), MAX_FRAMES_IN_FLIGHT);
	descriptorBoss->createDescriptorPool(MAX_FRAMES_IN_FLIGHT);
	descriptorBoss->createDescriptorSets(
		descriptorSetLayout,  // Use the member variable
		MAX_FRAMES_IN_FLIGHT,
		descriptorSets
	);

	descriptorBoss->updateDescriptorSets(
		descriptorSets,
		uniformBuffers,
		defaultMaterialUniformBuffers,
		textureImageView,
		textureSampler
	);

	createRayTracingDescriptorPool();
	createRayTracingDescriptorSet();

	auto sceneObjects = sceneLoader->getObjects();
	if (!sceneObjects.empty()) {
		loadModel(ASSETS_PATH + sceneObjects[0].modelPath);
	}
	else {
		loadModel(ASSETS_PATH "Car_Model/scene.gltf");
	}

	// 15. Create synchronization objects (semaphores and fences)
	createSyncObjects();

	// Initialize camera
	glm::vec3 camPos = sceneLoader->hasCameraSettings() ? sceneLoader->getInitialCameraPosition() : glm::vec3(0.0f, 0.0f, 3.0f);
	camera = std::make_unique<Camera>(camPos);

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
	swapChainImageLayouts.assign(imageCount, VK_IMAGE_LAYOUT_UNDEFINED);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkDevice vkDevice = device->getDevice();

	// Create per-frame synchronization objects
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(vkDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create per-frame synchronization objects!");
		}
		m_deletionQueue.pushSemaphore(vkDevice, imageAvailableSemaphores[i]);
		m_deletionQueue.pushFence(vkDevice, inFlightFences[i]);
	}

	// Create per-swapchain-image semaphores
	for (size_t i = 0; i < imageCount; i++) {
		if (vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create per-image synchronization objects!");
		}
		m_deletionQueue.pushSemaphore(vkDevice, renderFinishedSemaphores[i]);
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

void VulkanApplication::createDefaultMaterialUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(MaterialUBO);
	defaultMaterialUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	defaultMaterialUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	defaultMaterialUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

	MaterialUBO defaultMaterial{};
	defaultMaterial.metallicFactor = 0.0f;
	defaultMaterial.roughnessFactor = 1.0f;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		device->createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			defaultMaterialUniformBuffers[i],
			defaultMaterialUniformBuffersMemory[i]
		);
		vkMapMemory(device->getDevice(), defaultMaterialUniformBuffersMemory[i], 0, bufferSize, 0, &defaultMaterialUniformBuffersMapped[i]);
		memcpy(defaultMaterialUniformBuffersMapped[i], &defaultMaterial, sizeof(MaterialUBO));
	}
}

void VulkanApplication::createMaterialUniformBuffers()
{
	cleanupMaterialUniformBuffers();
	if (loadedModel.materials.empty()) {
		return;
	}

	VkDeviceSize bufferSize = sizeof(MaterialUBO);
	size_t materialCount = loadedModel.materials.size();
	materialUniformBuffers.resize(materialCount);
	materialUniformBuffersMemory.resize(materialCount);
	materialUniformBuffersMapped.resize(materialCount);

	for (size_t matIndex = 0; matIndex < materialCount; matIndex++) {
		materialUniformBuffers[matIndex].resize(MAX_FRAMES_IN_FLIGHT);
		materialUniformBuffersMemory[matIndex].resize(MAX_FRAMES_IN_FLIGHT);
		materialUniformBuffersMapped[matIndex].resize(MAX_FRAMES_IN_FLIGHT);

		MaterialUBO materialData{};
		materialData.metallicFactor = loadedModel.materials[matIndex].metallicFactor;
		materialData.roughnessFactor = loadedModel.materials[matIndex].roughnessFactor;

		for (size_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
			device->createBuffer(
				bufferSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				materialUniformBuffers[matIndex][frame],
				materialUniformBuffersMemory[matIndex][frame]
			);
			vkMapMemory(device->getDevice(), materialUniformBuffersMemory[matIndex][frame], 0, bufferSize, 0, &materialUniformBuffersMapped[matIndex][frame]);
			memcpy(materialUniformBuffersMapped[matIndex][frame], &materialData, sizeof(MaterialUBO));
		}
	}
}

void VulkanApplication::cleanupMaterialUniformBuffers()
{
	for (size_t matIndex = 0; matIndex < materialUniformBuffers.size(); matIndex++) {
		for (size_t frame = 0; frame < materialUniformBuffers[matIndex].size(); frame++) {
			if (materialUniformBuffers[matIndex][frame] != VK_NULL_HANDLE) {
				vkDestroyBuffer(device->getDevice(), materialUniformBuffers[matIndex][frame], nullptr);
				materialUniformBuffers[matIndex][frame] = VK_NULL_HANDLE;
			}
			if (materialUniformBuffersMemory[matIndex][frame] != VK_NULL_HANDLE) {
				vkFreeMemory(device->getDevice(), materialUniformBuffersMemory[matIndex][frame], nullptr);
				materialUniformBuffersMemory[matIndex][frame] = VK_NULL_HANDLE;
			}
		}
	}
	materialUniformBuffers.clear();
	materialUniformBuffersMemory.clear();
	materialUniformBuffersMapped.clear();
}

void VulkanApplication::updateUniformBuffer(uint32_t currentImage)
{
	UniformBufferObject ubo{};

	// Model matrix
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, modelTransform.position);
	model = glm::rotate(model, glm::radians(modelTransform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, glm::radians(modelTransform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::rotate(model, glm::radians(modelTransform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	model = glm::scale(model, glm::vec3(modelTransform.scale));

	ubo.model = model;
	ubo.view = camera->getViewMatrix();

	VkExtent2D extent = swapChain->getSwapChainExtent();
	float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
	ubo.proj = camera->getProjectionMatrix(aspect);

	// Normal matrix (inverse transpose of model matrix for correct normal transformation)
	ubo.normalMatrix = glm::transpose(glm::inverse(model));

	// Camera position for specular calculations
	ubo.viewPos = glm::vec4(camera->position, 1.0f);

	// Lighting data
	ubo.ambientStrength = ambientStrength;
	ubo.numLights = 0;

	for (size_t i = 0; i < lights.size() && i < MAX_LIGHTS; i++) {
		if (lights[i].enabled) {
			ubo.lights[ubo.numLights] = lights[i].toGPU();
			ubo.numLights++;
		}
	}

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

	if (currentRenderMode == RenderMode::RAYTRACING) {
		if (cameraMoved) {
			accumulationFrameCount = 0;
			cameraMoved = false;
		}
		updateRayTracingUniformBuffer();
		++accumulationFrameCount;
	}

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
       swapChainImageLayouts[imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	else if (currentRenderMode == RenderMode::RAYTRACING) {
		recordRayTracingCommandBuffer(commandBuffer, imageIndex);
		swapChainImageLayouts[imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
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
				pipelineLayout,
				loadedModel,
				skybox.get(),
	           swapChain->getSwapChainImages()[imageIndex],
	           swapChainImageLayouts[imageIndex],
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
	        swapChain->getSwapChainImages()[imageIndex],
				swapChainImageLayouts[imageIndex],
				descriptorSets,
				currentFrame,
				indexCount,
				*uiManager
			);
		}
		// Track swap chain image layout for graphics mode
		swapChainImageLayouts[imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
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

	cleanupAccumulationResources();

	// Cleanup old per-image semaphores
	for (size_t i = 0; i < renderFinishedSemaphores.size(); i++) {
		vkDestroySemaphore(device->getDevice(), renderFinishedSemaphores[i], nullptr);
	}

	// Cleanup old graphics pipeline
	graphicsPipeline.reset();
	additivePipeline.reset();

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
	createAccumulationImage();
	createRayTracingDescriptorSet();

	// IMPORTANT: Update compute pipeline descriptor sets with new image view
	if (computePipeline) {
		computePipeline->resetDesciriptorPool(device.get());
		computePipeline->createDescriptorSets(device.get(), computeOutputImageView);
	}

	// Recreate per-image semaphores for new swapchain
	size_t imageCount = swapChain->getSwapChainImages().size();
	renderFinishedSemaphores.resize(imageCount);
	imagesInFlight.resize(imageCount, VK_NULL_HANDLE);
	swapChainImageLayouts.assign(imageCount, VK_IMAGE_LAYOUT_UNDEFINED);

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
	uiManager = std::make_unique<UIManager>();
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
	uiManager->init(renderData, window.get());
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
		uiManager->renderLightingWindow(lights, ambientStrength);
		bool loadSceneFlag = false;
		uiManager->renderSceneLoader(
			loadSceneFlag,
			availableScenes,
			currentSceneIndex,
			[this](int sceneIndex) {
				if (sceneIndex < 0 || sceneIndex >= static_cast<int>(availableScenes.size())) {
					return;
				}

				if (!sceneLoader->loadScene(ASSETS_PATH + availableScenes[sceneIndex])) {
					return;
				}

				currentSceneIndex = sceneIndex;
				lights = sceneLoader->getLights();
				ambientStrength = sceneLoader->getConfig().ambientStrenght;
				if (lights.empty()) {
					setupDefaultLights();
				}

				const auto& sceneObjects = sceneLoader->getObjects();
				if (!sceneObjects.empty()) {
					if (modelLoaded) {
						vkDeviceWaitIdle(device->getDevice());

						objectLoader->destroyModel(loadedModel);
						modelLoaded = false;
						modelDescriptorSets.clear();
						cleanupMaterialUniformBuffers();
					}

					loadModel(ASSETS_PATH + sceneObjects[0].modelPath);

				}

				if (sceneLoader->hasCameraSettings()) {
					camera->position = sceneLoader->getInitialCameraPosition();
				}

				accumulationFrameCount = 0;
			}
		);
		glm::mat4 view = camera->getViewMatrix();
		VkExtent2D extent = swapChain->getSwapChainExtent();
		float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
		glm::mat4 proj = camera->getProjectionMatrix(aspect);
		uiManager->renderLightGizmo(lights, uiManager->getSelectedLight(),view,proj);
		drawFrame();
	}

	vkDeviceWaitIdle(device->getDevice());
}

void VulkanApplication::cleanup()
{
	// Discard DeletionQueue entries (manual cleanup handles destruction in correct order)
	m_deletionQueue.clear();

	// Cleanup in reverse order of creation
	cleanupComputeResources();

	// Cleanup uniform buffers (manually managed vectors)
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (uniformBuffers[i] != VK_NULL_HANDLE) {
			vkDestroyBuffer(device->getDevice(), uniformBuffers[i], nullptr);
		}
		if (uniformBuffersMemory[i] != VK_NULL_HANDLE) {
			vkFreeMemory(device->getDevice(), uniformBuffersMemory[i], nullptr);
		}
	}

	cleanupMaterialUniformBuffers();
	for (size_t i = 0; i < defaultMaterialUniformBuffers.size(); i++) {
		if (defaultMaterialUniformBuffers[i] != VK_NULL_HANDLE) {
			vkDestroyBuffer(device->getDevice(), defaultMaterialUniformBuffers[i], nullptr);
		}
		if (i < defaultMaterialUniformBuffersMemory.size() &&
			defaultMaterialUniformBuffersMemory[i] != VK_NULL_HANDLE) {
			vkFreeMemory(device->getDevice(), defaultMaterialUniformBuffersMemory[i], nullptr);
		}
	}

	// Cleanup per-image synchronization objects (leftover after DeletionQueue)
	for (size_t i = 0; i < renderFinishedSemaphores.size(); i++) {
		vkDestroySemaphore(device->getDevice(), renderFinishedSemaphores[i], nullptr);
	}

	// Destroy managers in correct dependency order
	descriptorBoss.reset();

	if (rayTracingUniformBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device->getDevice(), rayTracingUniformBuffer, nullptr);
		rayTracingUniformBuffer = VK_NULL_HANDLE;
	}
	if (rayTracingUniformBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device->getDevice(), rayTracingUniformBufferMemory, nullptr);
		rayTracingUniformBufferMemory = VK_NULL_HANDLE;
	}
	rayTracingUniformBufferMapped = nullptr;
	if (rayTracingDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device->getDevice(), rayTracingDescriptorPool, nullptr);
		rayTracingDescriptorPool = VK_NULL_HANDLE;
	}
	if (rayTracingDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(device->getDevice(), rayTracingDescriptorSetLayout, nullptr);
		rayTracingDescriptorSetLayout = VK_NULL_HANDLE;
	}

	commandBufferManager.reset();
	skybox.reset();
	graphicsPipeline.reset();
	additivePipeline.reset();

	if (pipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(device->getDevice(), pipelineLayout, nullptr);
		pipelineLayout = VK_NULL_HANDLE;
	}
	if (descriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(device->getDevice(), descriptorSetLayout, nullptr);
		descriptorSetLayout = VK_NULL_HANDLE;
	}

	swapChain.reset();
	renderPassObj.reset();
	textureManager.reset();
	bufferManager.reset();
	uiManager.reset();
	camera.reset();

	device.reset();
	window.reset();

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
		if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) {
			camera->processKeyboardInput(FORWARD, deltaTime);
			cameraMoved = true;
		}
		if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) {
			camera->processKeyboardInput(BACKWARD, deltaTime);
			cameraMoved = true;
		}
		if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) {
			camera->processKeyboardInput(LEFT, deltaTime);
			cameraMoved = true;
		}
		if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) {
			camera->processKeyboardInput(RIGHT, deltaTime);
			cameraMoved = true;
		}
		if (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS) {
			camera->processKeyboardInput(UP, deltaTime);
			cameraMoved = true;
		}
		if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
			camera->processKeyboardInput(DOWN, deltaTime);
			cameraMoved = true;
		}
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
	app->cameraMoved = true;
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

			// Material UBO (binding 2)
			VkDescriptorBufferInfo materialBufferInfo{};
			materialBufferInfo.buffer = materialUniformBuffers[matIndex][frame];
			materialBufferInfo.offset = 0;
			materialBufferInfo.range = sizeof(MaterialUBO);

			VkWriteDescriptorSet materialWrite{};
			materialWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			materialWrite.dstSet = modelDescriptorSets[matIndex][frame];
			materialWrite.dstBinding = 2;
			materialWrite.dstArrayElement = 0;
			materialWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			materialWrite.descriptorCount = 1;
			materialWrite.pBufferInfo = &materialBufferInfo;
			descriptorWrites.push_back(materialWrite);

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
void VulkanApplication::setupDefaultLights()
{
	// Clear existing lights
	lights.clear();

	// Add a directional light (like the sun)
	Light sunLight;
	sunLight.type = LightType::Directional;
	sunLight.direction = glm::vec3(-0.5f, -1.0f, -0.3f);
	sunLight.color = glm::vec3(1.0f, 0.95f, 0.8f);  // Warm white
	sunLight.intensity = 1.0f;
	sunLight.enabled = true;
	lights.push_back(sunLight);

	// Add a point light
	Light pointLight;
	pointLight.type = LightType::Point;
	pointLight.position = glm::vec3(2.0f, 3.0f, 2.0f);
	pointLight.color = glm::vec3(0.8f, 0.8f, 1.0f);  // Cool white
	pointLight.intensity = 2.0f;
	pointLight.constant = 1.0f;
	pointLight.linear = 0.09f;
	pointLight.quadratic = 0.032f;
	pointLight.enabled = true;
	lights.push_back(pointLight);

	std::cout << "Setup " << lights.size() << " lights" << std::endl;
}
void VulkanApplication::initComputePipeline() {
	computePipeline = std::make_unique<ComputePipeline>();

	// Create the output storage image for compute shader
	createComputeOutputImage();

	// Initialize compute pipeline components
	computePipeline->createDescriptorSetLayout(device.get());
	computePipeline->createDescriptorPool(device.get(), MAX_FRAMES_IN_FLIGHT);
	computePipeline->createDescriptorSets(device.get(), computeOutputImageView);
	computePipeline->createComputePipeline(device.get(), "Shaders/compute.comp.spv");

}

void VulkanApplication::initRayTracingPipeline()
{
	rayTracingPipeline = std::make_unique<RayTracingPipeline>();
	rayTracingPipeline->init(device.get());
	rayTracingPipeline->createPipeline(rayTracingDescriptorSetLayout);
	rayTracingPipeline->createShaderBindingTable();
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

	VkDevice vkDev = device->getDevice();

	if (vkCreateImage(vkDev, &imageInfo, nullptr, &computeOutputImage) != VK_SUCCESS) {
		throw std::runtime_error("failed to create compute output image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(vkDev, computeOutputImage, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(vkDev, &allocInfo, nullptr, &computeOutputImageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate compute output image memory!");
	}

	vkBindImageMemory(vkDev, computeOutputImage, computeOutputImageMemory, 0);

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

	if (vkCreateImageView(vkDev, &viewInfo, nullptr, &computeOutputImageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create compute output image view!");
	}

	m_deletionQueue.pushImage(vkDev, computeOutputImage, computeOutputImageMemory, computeOutputImageView);

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

void VulkanApplication::createAccumulationImage()
{
	VkExtent2D extent = swapChain->getSwapChainExtent();
	VkFormat accumFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = extent.width;
	imageInfo.extent.height = extent.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = accumFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkDevice vkDev = device->getDevice();

	if (vkCreateImage(vkDev, &imageInfo, nullptr, &accumOutputImage) != VK_SUCCESS) {
		throw std::runtime_error("failed to create accumulation image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(vkDev, accumOutputImage, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(vkDev, &allocInfo, nullptr, &accumOutputImageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate accumulation image memory!");
	}

	vkBindImageMemory(vkDev, accumOutputImage, accumOutputImageMemory, 0);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = accumOutputImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = accumFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(vkDev, &viewInfo, nullptr, &accumOutputImageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create accumulation image view!");
	}

	m_deletionQueue.pushImage(vkDev, accumOutputImage, accumOutputImageMemory, accumOutputImageView);

	VkCommandBuffer cmdBuffer = commandBufferManager->beginSingleTimeCommands();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = accumOutputImage;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

	vkCmdPipelineBarrier(
		cmdBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	commandBufferManager->endSingleTimeCommands(cmdBuffer);
}

void VulkanApplication::cleanupAccumulationResources()
{
	if (accumOutputImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(device->getDevice(), accumOutputImageView, nullptr);
		accumOutputImageView = VK_NULL_HANDLE;
	}
	if (accumOutputImage != VK_NULL_HANDLE) {
		vkDestroyImage(device->getDevice(), accumOutputImage, nullptr);
		accumOutputImage = VK_NULL_HANDLE;
	}
	if (accumOutputImageMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device->getDevice(), accumOutputImageMemory, nullptr);
		accumOutputImageMemory = VK_NULL_HANDLE;
	}
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
    swapBarrier.oldLayout = swapChainImageLayouts[imageIndex];
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

	// Transition swapchain image for color attachment rendering (UI)
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
	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record compute command buffer!");
	}
}

void VulkanApplication::recordRayTracingCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording ray tracing command buffer!");
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipeline->getPipeline());
	if (rayTracingDescriptorSet != VK_NULL_HANDLE) {
		vkCmdBindDescriptorSets(
			commandBuffer,
			VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
			rayTracingPipeline->getPipelineLayout(),
			0,
			1,
			&rayTracingDescriptorSet,
			0,
			nullptr
		);
	}

   const ShaderBindingTable& sbt = rayTracingPipeline->getSbt();
	uint32_t sbtStride = (sbt.handleSizeAligned + sbt.baseAlignment - 1) & ~(sbt.baseAlignment - 1);
	VkDeviceAddress sbtAddress = sbt.deviceAddress;

	VkStridedDeviceAddressRegionKHR rgenRegion{};
	rgenRegion.deviceAddress = sbtAddress;
	rgenRegion.stride = sbtStride;
	rgenRegion.size = sbtStride;

	VkStridedDeviceAddressRegionKHR missRegion{};
	missRegion.deviceAddress = sbtAddress + sbtStride;
	missRegion.stride = sbtStride;
	missRegion.size = sbtStride;

	VkStridedDeviceAddressRegionKHR hitRegion{};
	hitRegion.deviceAddress = sbtAddress + sbtStride * 2;
	hitRegion.stride = sbtStride;
	hitRegion.size = sbtStride;

	VkStridedDeviceAddressRegionKHR callableRegion{};

	VkExtent2D extent = swapChain->getSwapChainExtent();
	rayTracingPipeline->vkCmdTraceRaysKHRFunc(
		commandBuffer,
		&rgenRegion,
		&missRegion,
		&hitRegion,
		&callableRegion,
		extent.width,
		extent.height,
		1
	);

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
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	VkImageMemoryBarrier swapBarrier{};
	swapBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	swapBarrier.oldLayout = swapChainImageLayouts[imageIndex];
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

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
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
   VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(renderPassInfo.renderArea.extent.width);
	viewport.height = static_cast<float>(renderPassInfo.renderArea.extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = renderPassInfo.renderArea.extent;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	if (modelLoaded && !modelDescriptorSets.empty() && additivePipeline) {
		VkBuffer vertexBuffers[] = { loadedModel.vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, loadedModel.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		VkPipeline currentPipeline = VK_NULL_HANDLE;

		// Emissive pass with additive blending over ray-traced image
		for (const auto& mesh : loadedModel.transparentMeshIndices) {
			const auto& meshRef = loadedModel.meshes[mesh];
			for (const auto& primitive : meshRef.primitives) {
				int32_t matIndex = primitive.materialIndex >= 0 ? primitive.materialIndex : 0;
				if (matIndex >= static_cast<int32_t>(loadedModel.materials.size()) ||
					!loadedModel.materials[matIndex].isEmissive) {
					continue;
				}

				if (currentPipeline != additivePipeline->getGraphicsPipeline()) {
					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, additivePipeline->getGraphicsPipeline());
					currentPipeline = additivePipeline->getGraphicsPipeline();
				}

				if (matIndex < static_cast<int32_t>(modelDescriptorSets.size())) {
					vkCmdBindDescriptorSets(
						commandBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						pipelineLayout,
						0,
						1,
						&modelDescriptorSets[matIndex][currentFrame],
						0,
						nullptr
					);
				}

				vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
			}
		}
	}

	uiManager->render(commandBuffer);
	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record ray tracing command buffer!");
	}
}

void VulkanApplication::loadModel(const std::string& filepath)
{
	if (objectLoader->loadGLTF(filepath, loadedModel)) {
		objectLoader->createModelBuffers(loadedModel);
		createRayTracingGeometryBuffers();
     if (rayTracingAS) {
			rayTracingAS->buildBLAS(loadedModel);
			rayTracingAS->buildTLAS(loadedModel);
           createRayTracingDescriptorSet();
		}
		modelLoaded = true;
		indexCount = static_cast<uint32_t>(loadedModel.indices.size());
		createMaterialUniformBuffers();
		// create descriptor sets for the loaded model
		createModelDescriptorSets();
		std::cout << "Model loaded successfully: " << filepath << std::endl;
	}
	else {
		std::cerr << "Failed to load model: " << filepath << std::endl;
	}
}

void VulkanApplication::toggleRenderMode()
{
    if (currentRenderMode == RenderMode::GRAPHICS) {
		currentRenderMode = RenderMode::RAYTRACING;
		accumulationFrameCount = 0;
		std::cout << "Switched to Raytracing rendering mode" << std::endl;
	}
	//else if (currentRenderMode == RenderMode::COMPUTE) {
	//	currentRenderMode = RenderMode::RAYTRACING;
	//	std::cout << "Switched to Raytracing rendering mode" << std::endl;
	//}
	else {
		currentRenderMode = RenderMode::GRAPHICS;
		std::cout << "Switched to Graphics rendering mode" << std::endl;
	}
}

void VulkanApplication::cleanupComputeResources()
{
    if (computePipeline) {
        computePipeline->cleanup(device.get());
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
	cleanupAccumulationResources();
	if(objectLoader) {
		vkDeviceWaitIdle(device->getDevice());
		objectLoader->destroyModel(loadedModel);
		cleanupMaterialUniformBuffers();
		cleanupRayTracingGeometryBuffers();
	}
   if (rayTracingPipeline) {
		rayTracingPipeline->cleanup();
	}
}

void VulkanApplication::createRayTracingDescriptorSetLayout()
{
	std::array<VkDescriptorSetLayoutBinding, 10> bindings{};

	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	bindings[0].pImmutableSamplers = nullptr;

	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[1].pImmutableSamplers = nullptr;

	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[2].pImmutableSamplers = nullptr;

	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	bindings[3].pImmutableSamplers = nullptr;

	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	bindings[4].pImmutableSamplers = nullptr;

	bindings[5].binding = 5;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[5].descriptorCount = 1;
	bindings[5].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	bindings[5].pImmutableSamplers = nullptr;

	bindings[6].binding = 6;
	bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[6].descriptorCount = 1;
	bindings[6].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	bindings[6].pImmutableSamplers = nullptr;

	bindings[7].binding = 7;
	bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[7].descriptorCount = 1;
	bindings[7].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[7].pImmutableSamplers = nullptr;

	bindings[8].binding = 8;
	bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[8].descriptorCount = 16;
	bindings[8].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	bindings[8].pImmutableSamplers = nullptr;

	bindings[9].binding = 9;
	bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[9].descriptorCount = 1;
	bindings[9].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[9].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VkDevice vkDev = device->getDevice();
	if (vkCreateDescriptorSetLayout(vkDev, &layoutInfo, nullptr, &rayTracingDescriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create ray tracing descriptor set layout!");
	}
	m_deletionQueue.pushDescriptorSetLayout(vkDev, rayTracingDescriptorSetLayout);
}

void VulkanApplication::createRayTracingDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 5> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[1].descriptorCount = 2;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[2].descriptorCount = 1;
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[3].descriptorCount = 4;
	poolSizes[4].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[4].descriptorCount = 17;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1;

	VkDevice vkDev = device->getDevice();
	if (vkCreateDescriptorPool(vkDev, &poolInfo, nullptr, &rayTracingDescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create ray tracing descriptor pool!");
	}
	m_deletionQueue.pushDescriptorPool(vkDev, rayTracingDescriptorPool);
}

void VulkanApplication::createRayTracingDescriptorSet()
{
	if (!rayTracingAS || rayTracingAS->getTLAS().handle == VK_NULL_HANDLE) {
		return;
	}
	if (loadedModel.indexBuffer == VK_NULL_HANDLE || loadedModel.rtVertexBuffer == VK_NULL_HANDLE ||
		rayTracingPrimitiveBuffer == VK_NULL_HANDLE || rayTracingMeshBuffer == VK_NULL_HANDLE) {
		return;
	}

    if (rayTracingDescriptorSet == VK_NULL_HANDLE) {
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = rayTracingDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &rayTracingDescriptorSetLayout;

		if (vkAllocateDescriptorSets(device->getDevice(), &allocInfo, &rayTracingDescriptorSet) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate ray tracing descriptor set!");
		}
	}

	VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
	asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	asInfo.accelerationStructureCount = 1;
	VkAccelerationStructureKHR tlasHandle = rayTracingAS->getTLAS().handle;
	asInfo.pAccelerationStructures = &tlasHandle;

	VkWriteDescriptorSet asWrite{};
	asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	asWrite.dstSet = rayTracingDescriptorSet;
	asWrite.dstBinding = 0;
	asWrite.descriptorCount = 1;
	asWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	asWrite.pNext = &asInfo;

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = computeOutputImageView;
	imageInfo.sampler = VK_NULL_HANDLE;

	VkDescriptorBufferInfo cameraBufferInfo{};
	cameraBufferInfo.buffer = rayTracingUniformBuffer;
	cameraBufferInfo.offset = 0;
	cameraBufferInfo.range = sizeof(glm::mat4) * 5 + sizeof(glm::vec4) + sizeof(GPULight) * MAX_LIGHTS + sizeof(glm::vec4);

	VkWriteDescriptorSet imageWrite{};
	imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	imageWrite.dstSet = rayTracingDescriptorSet;
	imageWrite.dstBinding = 1;
	imageWrite.descriptorCount = 1;
	imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	imageWrite.pImageInfo = &imageInfo;

	VkWriteDescriptorSet cameraWrite{};
	cameraWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	cameraWrite.dstSet = rayTracingDescriptorSet;
	cameraWrite.dstBinding = 2;
	cameraWrite.descriptorCount = 1;
	cameraWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraWrite.pBufferInfo = &cameraBufferInfo;

	VkDescriptorBufferInfo indexBufferInfo{};
	indexBufferInfo.buffer = loadedModel.indexBuffer;
	indexBufferInfo.offset = 0;
	indexBufferInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo vertexBufferInfo{};
	vertexBufferInfo.buffer = loadedModel.rtVertexBuffer;
	vertexBufferInfo.offset = 0;
	vertexBufferInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo primitiveBufferInfo{};
	primitiveBufferInfo.buffer = rayTracingPrimitiveBuffer;
	primitiveBufferInfo.offset = 0;
	primitiveBufferInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo meshBufferInfo{};
	meshBufferInfo.buffer = rayTracingMeshBuffer;
	meshBufferInfo.offset = 0;
	meshBufferInfo.range = VK_WHOLE_SIZE;

	VkWriteDescriptorSet indexWrite{};
	indexWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	indexWrite.dstSet = rayTracingDescriptorSet;
	indexWrite.dstBinding = 3;
	indexWrite.descriptorCount = 1;
	indexWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	indexWrite.pBufferInfo = &indexBufferInfo;

	VkWriteDescriptorSet vertexWrite{};
	vertexWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	vertexWrite.dstSet = rayTracingDescriptorSet;
	vertexWrite.dstBinding = 4;
	vertexWrite.descriptorCount = 1;
	vertexWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	vertexWrite.pBufferInfo = &vertexBufferInfo;

	VkWriteDescriptorSet primitiveWrite{};
	primitiveWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	primitiveWrite.dstSet = rayTracingDescriptorSet;
	primitiveWrite.dstBinding = 5;
	primitiveWrite.descriptorCount = 1;
	primitiveWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	primitiveWrite.pBufferInfo = &primitiveBufferInfo;

	VkWriteDescriptorSet meshWrite{};
	meshWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	meshWrite.dstSet = rayTracingDescriptorSet;
	meshWrite.dstBinding = 6;
	meshWrite.descriptorCount = 1;
	meshWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	meshWrite.pBufferInfo = &meshBufferInfo;

	VkWriteDescriptorSet cubemapWrite{};
	{
		VkDescriptorImageInfo cubemapImageInfo{};
		cubemapImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		cubemapImageInfo.imageView = textureImageView;
		cubemapImageInfo.sampler = textureSampler;
		if (skybox && skybox->getCubemapImageView() != VK_NULL_HANDLE) {
			cubemapImageInfo.imageView = skybox->getCubemapImageView();
			cubemapImageInfo.sampler = skybox->getCubemapSampler();
		}
		cubemapWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cubemapWrite.dstSet = rayTracingDescriptorSet;
		cubemapWrite.dstBinding = 7;
		cubemapWrite.descriptorCount = 1;
		cubemapWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		cubemapWrite.pImageInfo = &cubemapImageInfo;
	}

	VkWriteDescriptorSet textureWrite{};
	std::vector<VkDescriptorImageInfo> texImageInfos(16);
	for (size_t i = 0; i < 16; i++) {
		texImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		texImageInfos[i].imageView = textureImageView;
		texImageInfos[i].sampler = textureSampler;
	}
	for (size_t i = 0; i < loadedModel.textures.size() && i < 16; i++) {
		if (loadedModel.textures[i].imageView != VK_NULL_HANDLE) {
			texImageInfos[i].imageView = loadedModel.textures[i].imageView;
			texImageInfos[i].sampler = loadedModel.textures[i].sampler;
		}
	}
	textureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	textureWrite.dstSet = rayTracingDescriptorSet;
	textureWrite.dstBinding = 8;
	textureWrite.dstArrayElement = 0;
	textureWrite.descriptorCount = 16;
	textureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureWrite.pImageInfo = texImageInfos.data();

	VkDescriptorImageInfo accumImageInfo{};
	accumImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	accumImageInfo.imageView = accumOutputImageView;
	accumImageInfo.sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet accumWrite{};
	accumWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	accumWrite.dstSet = rayTracingDescriptorSet;
	accumWrite.dstBinding = 9;
	accumWrite.descriptorCount = 1;
	accumWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	accumWrite.pImageInfo = &accumImageInfo;

    std::array<VkWriteDescriptorSet, 10> writes{ asWrite, imageWrite, cameraWrite, indexWrite, vertexWrite, primitiveWrite, meshWrite, cubemapWrite, textureWrite, accumWrite };
	vkUpdateDescriptorSets(device->getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanApplication::createDescriptorSetLayout()
{
	// UBO binding (binding = 0)
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	// Sampler binding (binding = 1)
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	// Material UBO binding (binding = 2)
	VkDescriptorSetLayoutBinding materialLayoutBinding{};
	materialLayoutBinding.binding = 2;
	materialLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	materialLayoutBinding.descriptorCount = 1;
	materialLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	materialLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
		uboLayoutBinding,
		samplerLayoutBinding,
		materialLayoutBinding
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VkDevice vkDev = device->getDevice();
	if (vkCreateDescriptorSetLayout(vkDev, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
	m_deletionQueue.pushDescriptorSetLayout(vkDev, descriptorSetLayout);
}

void VulkanApplication::createPipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	VkDevice vkDev = device->getDevice();
	if (vkCreatePipelineLayout(vkDev, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}
	m_deletionQueue.pushPipelineLayout(vkDev, pipelineLayout);
}

void VulkanApplication::createGraphicsPipeline()
{
	PipelineConfigInfo pipelineConfig{};
	VulkanPipeline::defaultPipelineConfigInfo(pipelineConfig);
	pipelineConfig.renderPass = renderPass;
	pipelineConfig.pipelineLayout = pipelineLayout;
	VulkanPipeline::enableAlphaBlending(pipelineConfig);
	graphicsPipeline = std::make_unique<VulkanPipeline>(
		device.get(),
		"Shaders/shader.vert.spv",
		"Shaders/brdf.frag.spv",
		pipelineConfig
	);
	PipelineConfigInfo additiveConfig{};
	VulkanPipeline::defaultPipelineConfigInfo(additiveConfig);
	additiveConfig.renderPass = renderPass;
	additiveConfig.pipelineLayout = pipelineLayout;
	VulkanPipeline::enableAdditiveBlending(additiveConfig);
	additivePipeline = std::make_unique<VulkanPipeline>(
		device.get(),
		"Shaders/shader.vert.spv",
		"Shaders/brdf.frag.spv",
		additiveConfig
	);
}
