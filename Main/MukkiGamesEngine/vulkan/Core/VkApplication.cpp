#include "VkApplication.h"
#include "EngineWindow.h"
#include "../objects/vertex.h"
#include <stdexcept>
#include <array>
#include "ShaderCompiler.h"
#include <iostream>
#include "../pipeline/computePipeline.h"
#include "../Physics/VehiclePhysics.h"


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

	LoadedObject* rtObj = nullptr;
	for (auto& obj : loadedObjects) {
		if (obj.loaded && !obj.model.meshes.empty()) {
			rtObj = &obj;
			break;
		}
	}
	if (!rtObj) return;

	Model& rtModel = rtObj->model;

	std::vector<RayTracingPrimitiveInfo> primitiveInfos;
	primitiveInfos.reserve(rtModel.indices.size() / 3);
	std::vector<RayTracingMeshInfo> meshInfos;
	meshInfos.reserve(rtModel.meshes.size());

	uint32_t primitiveOffset = 0;
	for (const auto& mesh : rtModel.meshes) {
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
				primitive.materialIndex < static_cast<int32_t>(rtModel.materials.size())) {
				const auto& mat = rtModel.materials[primitive.materialIndex];
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
	LoadedObject* rtObj = nullptr;
	for (auto& obj : loadedObjects) {
		if (obj.loaded) {
			rtObj = &obj;
			break;
		}
	}
	if (rtObj) {
		model = rtObj->transform.getModelMatrix();
	}
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
	shadowMap = std::make_unique<ShadowMap>();
	shadowMap->init(device.get(), 2048);

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
		textureSampler,
		shadowMap ? shadowMap->getShadowMapImageView() : VK_NULL_HANDLE,
		shadowMap ? shadowMap->getShadowSampler() : VK_NULL_HANDLE
	);

	createRayTracingDescriptorPool();
	createRayTracingDescriptorSet();

	loadSceneObjects();
	initPhysics();

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



void VulkanApplication::updateUniformBuffer(uint32_t currentImage)
{
	UniformBufferObject ubo{};
	ubo.model = glm::mat4(1.0f);
	ubo.view = camera->getViewMatrix();

	VkExtent2D extent = swapChain->getSwapChainExtent();
	float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
	ubo.proj = camera->getProjectionMatrix(aspect);
	ubo.normalMatrix = glm::mat4(1.0f);

	ubo.lightSpaceMatrix = glm::mat4(1.0f);
	for (const auto& light : lights) {
		if (light.enabled && light.type == LightType::Directional) {
			ubo.lightSpaceMatrix = computeDirectionalLightSpaceMatrix(light, *camera);
			break;
		}
	}

	ubo.viewPos = glm::vec4(camera->position, 1.0f);
	ubo.ambientStrength = ambientStrength;
	ubo.numLights = 0;

	for (size_t i = 0; i < lights.size() && i < MAX_LIGHTS; i++) {
		if (lights[i].enabled) {
			ubo.lights[ubo.numLights] = lights[i].toGPU();
			ubo.numLights++;
		}
	}

	if (shadowMap) {
		ubo.padding[0] = 1.0f / static_cast<float>(shadowMap->getShadowMapSize());
	}

	memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void VulkanApplication::updatePerObjectUBO(LoadedObject& obj, uint32_t currentImage)
{
	UniformBufferObject ubo{};

	glm::mat4 model = obj.transform.getModelMatrix();
	ubo.model = model;
	ubo.view = camera->getViewMatrix();

	VkExtent2D extent = swapChain->getSwapChainExtent();
	float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
	ubo.proj = camera->getProjectionMatrix(aspect);
	ubo.normalMatrix = glm::transpose(glm::inverse(model));

	ubo.lightSpaceMatrix = glm::mat4(1.0f);
	for (const auto& light : lights) {
		if (light.enabled && light.type == LightType::Directional) {
			ubo.lightSpaceMatrix = computeDirectionalLightSpaceMatrix(light, *camera);
			break;
		}
	}

	ubo.viewPos = glm::vec4(camera->position, 1.0f);
	ubo.ambientStrength = ambientStrength;
	ubo.numLights = 0;

	for (size_t i = 0; i < lights.size() && i < MAX_LIGHTS; i++) {
		if (lights[i].enabled) {
			ubo.lights[ubo.numLights] = lights[i].toGPU();
			ubo.numLights++;
		}
	}

	if (shadowMap) {
		ubo.padding[0] = 1.0f / static_cast<float>(shadowMap->getShadowMapSize());
	}

	memcpy(obj.uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
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

	auto loadedObjCount = loadedObjects.size();
	bool hasLoadedModels = loadedObjCount > 0;

	if (hasLoadedModels) {
		for (auto& obj : loadedObjects) {
			if (obj.loaded) {
				updatePerObjectUBO(obj, currentFrame);
			}
		}
	} else {
		updateUniformBuffer(currentFrame);
	}

	if (skybox) {
		VkExtent2D extent = swapChain->getSwapChainExtent();
		float aspect = extent.width / (float)extent.height;
		glm::mat4 view = camera->getSkyboxVPMatrix(aspect, 0.1f, 100.f);
		skybox->updateUniformBuffer(currentFrame, view);
	}

	// Record shadow pass for directional light shadow mapping
	recordShadowPass();

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
		if (hasLoadedModels) {
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
				throw std::runtime_error("failed to begin recording command buffer!");
			}

			VkPipeline mainPipeline = graphicsPipeline->getGraphicsPipeline();
			VkPipeline addPipeline = additivePipeline ? additivePipeline->getGraphicsPipeline() : VK_NULL_HANDLE;

			commandBufferManager->beginModelRenderPass(
				commandBuffer,
				renderPass,
				swapChain->getSwapChainFramebuffers()[imageIndex],
				swapChain->getSwapChainExtent(),
				mainPipeline,
				pipelineLayout,
				skybox.get(),
				swapChain->getSwapChainImages()[imageIndex],
				swapChainImageLayouts[imageIndex],
				currentFrame);

			for (auto& obj : loadedObjects) {
				if (obj.loaded) {
					commandBufferManager->recordModelDrawCommands(
						commandBuffer,
						obj.model,
						pipelineLayout,
						mainPipeline,
						addPipeline,
						obj.descriptorSets,
						currentFrame);
				}
			}

			commandBufferManager->endModelRenderPass(commandBuffer, *uiManager);

			if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
				throw std::runtime_error("failed to record command buffer!");
			}
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

		if (physicsEngine) {
			static float accumulator = 0.0f;
			const float fixedDt = 1.0f / 60.0f;
			accumulator += deltaTime;
			if (accumulator > 0.1f) accumulator = 0.1f;
			while (accumulator >= fixedDt) {
				physicsEngine->step(fixedDt);
				accumulator -= fixedDt;
			}
			syncPhysicsTransforms();
		}

		uiManager->newFrame();
		float fps = 1.0f / deltaTime;
		uiManager->renderDebugWindow(fps, deltaTime);
		uiManager->renderCameraInfo(camera->position, camera->front);
		{
			std::vector<std::string> objNames;
			const auto& sceneObjs = sceneLoader->getObjects();
			for (size_t i = 0; i < loadedObjects.size(); i++) {
				std::string name = "Unnamed";
				if (i < sceneObjs.size()) {
					name = sceneObjs[i].name;
				}
				objNames.push_back(name);
			}
			Transform* targetTransform = nullptr;
			if (!loadedObjects.empty()) {
				if (selectedObjectIndex < 0 || selectedObjectIndex >= static_cast<int>(loadedObjects.size())) {
					selectedObjectIndex = 0;
				}
				targetTransform = &loadedObjects[selectedObjectIndex].transform;
			}
			glm::vec3 pos = targetTransform ? targetTransform->position : glm::vec3(0.0f);
			glm::vec3 rot = targetTransform ? targetTransform->rotation : glm::vec3(0.0f);
			glm::vec3 scl = targetTransform ? targetTransform->scale : glm::vec3(1.0f);
			uiManager->renderObjectTransformWindow(objNames, selectedObjectIndex, pos, rot, scl, deltaTime);
			if (targetTransform) {
				targetTransform->position = pos;
				targetTransform->rotation = rot;
				targetTransform->scale = scl;
			}
		}
		uiManager->renderLightingWindow(lights, ambientStrength);
		{	bool resetAcc = false;
			uiManager->renderRayTracingControls(resetAcc);
			if (resetAcc) {
				accumulationFrameCount = 0;
			}
		}
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

				loadSceneObjects();
				initPhysics();

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

	// Cleanup per-frame synchronization objects
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
			vkDestroySemaphore(device->getDevice(), imageAvailableSemaphores[i], nullptr);
		}
		if (inFlightFences[i] != VK_NULL_HANDLE) {
			vkDestroyFence(device->getDevice(), inFlightFences[i], nullptr);
		}
	}

	// Cleanup per-image synchronization objects
	for (size_t i = 0; i < renderFinishedSemaphores.size(); i++) {
		if (renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
			vkDestroySemaphore(device->getDevice(), renderFinishedSemaphores[i], nullptr);
		}
	}

	// Cleanup uniform buffers (manually managed vectors)
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (uniformBuffers[i] != VK_NULL_HANDLE) {
			vkDestroyBuffer(device->getDevice(), uniformBuffers[i], nullptr);
		}
		if (uniformBuffersMemory[i] != VK_NULL_HANDLE) {
			vkFreeMemory(device->getDevice(), uniformBuffersMemory[i], nullptr);
		}
	}

	vkDeviceWaitIdle(device->getDevice());
	destroyAllLoadedObjects();
	if (physicsEngine) {
		physicsEngine->shutdown();
		physicsEngine.reset();
	}
	for (size_t i = 0; i < defaultMaterialUniformBuffers.size(); i++) {
		if (defaultMaterialUniformBuffers[i] != VK_NULL_HANDLE) {
			vkDestroyBuffer(device->getDevice(), defaultMaterialUniformBuffers[i], nullptr);
		}
		if (i < defaultMaterialUniformBuffersMemory.size() &&
			defaultMaterialUniformBuffersMemory[i] != VK_NULL_HANDLE) {
			vkFreeMemory(device->getDevice(), defaultMaterialUniformBuffersMemory[i], nullptr);
		}
	}

	// Cleanup texture resources
	if (textureSampler != VK_NULL_HANDLE && textureManager) {
		textureManager->destroySampler(textureSampler);
	}
	if (textureImageView != VK_NULL_HANDLE && textureManager) {
		textureManager->destroyImageView(textureImageView);
	}
	if (textureImage != VK_NULL_HANDLE && textureManager) {
		textureManager->destroyImage(textureImage, textureImageMemory);
	}

	// Cleanup depth resources
	if (depthImageView != VK_NULL_HANDLE && textureManager) {
		textureManager->destroyImageView(depthImageView);
	}
	if (depthImage != VK_NULL_HANDLE && textureManager) {
		textureManager->destroyImage(depthImage, depthImageMemory);
	}

	// Cleanup vertex/index buffers (fallback quad)
	if (indexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device->getDevice(), indexBuffer, nullptr);
	}
	if (indexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device->getDevice(), indexBufferMemory, nullptr);
	}
	if (vertexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device->getDevice(), vertexBuffer, nullptr);
	}
	if (vertexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device->getDevice(), vertexBufferMemory, nullptr);
	}

	// TAA cleanup
	if (taaPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(device->getDevice(), taaPipelineLayout, nullptr);
	}
	if (taaDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(device->getDevice(), taaDescriptorSetLayout, nullptr);
	}
	if (taaDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device->getDevice(), taaDescriptorPool, nullptr);
	}
	if (taaPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(device->getDevice(), taaPipeline, nullptr);
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
	if (shadowMap) {
		shadowMap->cleanup();
		shadowMap.reset();
	}
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

// Helper: compute orthographic light-space matrix for directional light shadow mapping
glm::mat4 VulkanApplication::computeDirectionalLightSpaceMatrix(const Light& light, const Camera& camera){
    float near = 0.1f, far = 80.0f;
    float orthoSize = 25.0f;
    glm::vec3 lightPos = camera.position - light.direction * 40.0f;
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(glm::normalize(light.direction), worldUp)) > 0.99f) {
        worldUp = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    glm::mat4 lightView = glm::lookAt(lightPos, camera.position, worldUp);
    glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, near, far);
    return lightProj * lightView;
}

void VulkanApplication::recordShadowPass()
{
    if (!shadowMap) return;

    // Find first enabled directional light
    const Light* dirLight = nullptr;
    for (const auto& light : lights) {
        if (light.enabled && light.type == LightType::Directional) {
            dirLight = &light;
            break;
        }
    }
    if (!dirLight) return;

    glm::mat4 lightSpaceMatrix = computeDirectionalLightSpaceMatrix(*dirLight, *camera);

    VkCommandBuffer cmd = commandBufferManager->beginSingleTimeCommands();

    // Transition shadow images to proper rendering layouts
    VkImageMemoryBarrier shadowColorBarrier{};
    shadowColorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    shadowColorBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    shadowColorBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    shadowColorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shadowColorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shadowColorBarrier.image = shadowMap->getShadowImage();
    shadowColorBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    shadowColorBarrier.subresourceRange.baseMipLevel = 0;
    shadowColorBarrier.subresourceRange.levelCount = 1;
    shadowColorBarrier.subresourceRange.baseArrayLayer = 0;
    shadowColorBarrier.subresourceRange.layerCount = 1;
    shadowColorBarrier.srcAccessMask = 0;
    shadowColorBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkImageMemoryBarrier shadowDepthBarrier{};
    shadowDepthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    shadowDepthBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    shadowDepthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    shadowDepthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shadowDepthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shadowDepthBarrier.image = shadowMap->getDepthImage();
    shadowDepthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    shadowDepthBarrier.subresourceRange.baseMipLevel = 0;
    shadowDepthBarrier.subresourceRange.levelCount = 1;
    shadowDepthBarrier.subresourceRange.baseArrayLayer = 0;
    shadowDepthBarrier.subresourceRange.layerCount = 1;
    shadowDepthBarrier.srcAccessMask = 0;
    shadowDepthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkImageMemoryBarrier barriers[] = { shadowColorBarrier, shadowDepthBarrier };
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        0,
        0, nullptr,
        0, nullptr,
        2, barriers
    );

    // Begin shadow render pass
    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = shadowMap->getRenderPass();
    rpInfo.framebuffer = shadowMap->getFramebuffer();
    rpInfo.renderArea.offset = { 0, 0 };
    rpInfo.renderArea.extent = shadowMap->getExtent();

    VkClearValue clearValues[2] = {};
    clearValues[0].color = { {1.0f, 0.0f, 0.0f, 0.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };
    rpInfo.clearValueCount = 2;
    rpInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowMap->getPipeline());

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(shadowMap->getShadowMapSize());
    viewport.height = static_cast<float>(shadowMap->getShadowMapSize());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = shadowMap->getExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Set depth bias (dynamic state) to match the pipeline's configured values
    vkCmdSetDepthBias(cmd, 1.25f, 0.0f, 1.75f);

    // Push light-space matrix via push constant
    vkCmdPushConstants(cmd, shadowMap->getPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &lightSpaceMatrix);

    // Draw geometry into shadow map
    bool drewAnyModel = false;
    for (const auto& obj : loadedObjects) {
        if (obj.loaded && obj.model.vertexBuffer != VK_NULL_HANDLE) {
            VkBuffer vertexBuffers[] = { obj.model.vertexBuffer };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(cmd, obj.model.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            for (const auto& meshIndex : obj.model.opaqueMeshIndices) {
                const auto& meshRef = obj.model.meshes[meshIndex];
                for (const auto& primitive : meshRef.primitives) {
                    vkCmdDrawIndexed(cmd, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
                }
            }
            drewAnyModel = true;
        }
    }
    if (!drewAnyModel) {
        VkBuffer vertexBuffers[] = { vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmd);
    commandBufferManager->endSingleTimeCommands(cmd);
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

	vehicleThrottle = 0.0f;
	vehicleBrake = 0.0f;
	vehicleSteering = 0.0f;

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

		// Vehicle controls (arrow keys)
		if (glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS) {
			vehicleThrottle = 500.0f;
		}
		if (glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS) {
			vehicleBrake = 500.0f;
		}
		if (glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS) {
			vehicleSteering = 5.0f;
		}
		if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) {
			vehicleSteering = -5.0f;
		}
	}

	// Apply vehicle input
	for (auto& obj : loadedObjects) {
		if (obj.vehicle) {
			obj.vehicle->setInput(vehicleThrottle, vehicleBrake, vehicleSteering);
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

void VulkanApplication::setupDefaultLights()
{
	// Clear existing lights
	lights.clear();

	// Add a directional light (like the sun)
	Light sunLight;
	sunLight.type = LightType::Directional;
	sunLight.direction = glm::vec3(-0.8f, -0.5f, -0.3f);
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

	LoadedObject* rtObj = nullptr;
	for (auto& obj : loadedObjects) {
		if (obj.loaded && !obj.descriptorSets.empty() && additivePipeline) {
			rtObj = &obj;
			break;
		}
	}
	if (rtObj) {
		Model& rtModel = rtObj->model;
		VkBuffer vertexBuffers[] = { rtModel.vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, rtModel.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		VkPipeline currentPipeline = VK_NULL_HANDLE;

		// Emissive pass with additive blending over ray-traced image
		for (const auto& mesh : rtModel.transparentMeshIndices) {
			const auto& meshRef = rtModel.meshes[mesh];
			for (const auto& primitive : meshRef.primitives) {
				int32_t matIndex = primitive.materialIndex >= 0 ? primitive.materialIndex : 0;
				if (matIndex >= static_cast<int32_t>(rtModel.materials.size()) ||
					!rtModel.materials[matIndex].isEmissive) {
					continue;
				}

				if (currentPipeline != additivePipeline->getGraphicsPipeline()) {
					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, additivePipeline->getGraphicsPipeline());
					currentPipeline = additivePipeline->getGraphicsPipeline();
				}

				if (matIndex < static_cast<int32_t>(rtObj->descriptorSets.size())) {
					vkCmdBindDescriptorSets(
						commandBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						pipelineLayout,
						0,
						1,
						&rtObj->descriptorSets[matIndex][currentFrame],
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

void VulkanApplication::loadSceneObjects()
{
	const auto& sceneObjects = sceneLoader->getObjects();
	destroyAllLoadedObjects();
	for (const auto& sceneObj : sceneObjects) {
		if (!sceneObj.modelPath.empty()) {
			LoadedObject obj = createLoadedObject(sceneObj);
			if (obj.loaded) {
				loadedObjects.push_back(std::move(obj));
			}
		}
	}
	createRayTracingGeometryBuffers();
	if (rayTracingAS) {
		for (auto& obj : loadedObjects) {
			if (obj.loaded) {
				rayTracingAS->buildBLAS(obj.model);
				rayTracingAS->buildTLAS(obj.model);
				break; // Only first object for RT
			}
		}
		createRayTracingDescriptorSet();
	}
}

LoadedObject VulkanApplication::createLoadedObject(const SceneObject& sceneObj)
{
	LoadedObject obj;
	obj.transform = sceneObj.modelTransform;
	obj.physics = sceneObj.physics;
	obj.sceneObjectId = sceneObj.id;

	std::string fullPath = std::string(ASSETS_PATH) + sceneObj.modelPath;
	if (!objectLoader->loadGLTF(fullPath, obj.model)) {
		std::cerr << "Failed to load model: " << fullPath << std::endl;
		return obj;
	}

	objectLoader->createModelBuffers(obj.model);

	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	obj.uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	obj.uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	obj.uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		device->createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			obj.uniformBuffers[i],
			obj.uniformBuffersMemory[i]);
		vkMapMemory(device->getDevice(), obj.uniformBuffersMemory[i], 0, bufferSize, 0, &obj.uniformBuffersMapped[i]);
	}

	if (!obj.model.materials.empty()) {
		VkDeviceSize matBufferSize = sizeof(MaterialUBO);
		size_t materialCount = obj.model.materials.size();
		obj.materialUniformBuffers.resize(materialCount);
		obj.materialUniformBuffersMemory.resize(materialCount);
		obj.materialUniformBuffersMapped.resize(materialCount);

		for (size_t matIndex = 0; matIndex < materialCount; matIndex++) {
			obj.materialUniformBuffers[matIndex].resize(MAX_FRAMES_IN_FLIGHT);
			obj.materialUniformBuffersMemory[matIndex].resize(MAX_FRAMES_IN_FLIGHT);
			obj.materialUniformBuffersMapped[matIndex].resize(MAX_FRAMES_IN_FLIGHT);

			MaterialUBO materialData{};
			materialData.metallicFactor = obj.model.materials[matIndex].metallicFactor;
			materialData.roughnessFactor = obj.model.materials[matIndex].roughnessFactor;

			for (size_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
				device->createBuffer(
					matBufferSize,
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					obj.materialUniformBuffers[matIndex][frame],
					obj.materialUniformBuffersMemory[matIndex][frame]);
				vkMapMemory(device->getDevice(), obj.materialUniformBuffersMemory[matIndex][frame], 0, matBufferSize, 0, &obj.materialUniformBuffersMapped[matIndex][frame]);
				memcpy(obj.materialUniformBuffersMapped[matIndex][frame], &materialData, sizeof(MaterialUBO));
			}
		}
	}

	size_t materialCount = obj.model.materials.empty() ? 1 : obj.model.materials.size();
	obj.descriptorSets.resize(materialCount);
	for (size_t matIndex = 0; matIndex < materialCount; matIndex++) {
		obj.descriptorSets[matIndex].resize(MAX_FRAMES_IN_FLIGHT);

		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorBoss->getDescriptorPool();
		allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
		allocInfo.pSetLayouts = layouts.data();

		if (vkAllocateDescriptorSets(device->getDevice(), &allocInfo, obj.descriptorSets[matIndex].data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate descriptor sets for loaded object!");
		}

		const auto& material = (matIndex < obj.model.materials.size()) ? obj.model.materials[matIndex] : Material{};
		for (size_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
			std::vector<VkWriteDescriptorSet> descriptorWrites;

			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = obj.uniformBuffers[frame];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniformBufferObject);

			VkWriteDescriptorSet uboWrite{};
			uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			uboWrite.dstSet = obj.descriptorSets[matIndex][frame];
			uboWrite.dstBinding = 0;
			uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			uboWrite.descriptorCount = 1;
			uboWrite.pBufferInfo = &bufferInfo;
			descriptorWrites.push_back(uboWrite);

			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			if (material.baseColorTextureIndex >= 0 &&
				material.baseColorTextureIndex < static_cast<int32_t>(obj.model.textures.size())) {
				const LoadedTexture& tex = obj.model.textures[material.baseColorTextureIndex];
				imageInfo.imageView = tex.imageView;
				imageInfo.sampler = tex.sampler;
			} else {
				imageInfo.imageView = textureImageView;
				imageInfo.sampler = textureSampler;
			}

			VkWriteDescriptorSet samplerWrite{};
			samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			samplerWrite.dstSet = obj.descriptorSets[matIndex][frame];
			samplerWrite.dstBinding = 1;
			samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			samplerWrite.descriptorCount = 1;
			samplerWrite.pImageInfo = &imageInfo;
			descriptorWrites.push_back(samplerWrite);

			VkDescriptorBufferInfo materialBufferInfo{};
			materialBufferInfo.buffer = obj.materialUniformBuffers.empty() ? defaultMaterialUniformBuffers[frame] : obj.materialUniformBuffers[matIndex][frame];
			materialBufferInfo.offset = 0;
			materialBufferInfo.range = sizeof(MaterialUBO);

			VkWriteDescriptorSet materialWrite{};
			materialWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			materialWrite.dstSet = obj.descriptorSets[matIndex][frame];
			materialWrite.dstBinding = 2;
			materialWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			materialWrite.descriptorCount = 1;
			materialWrite.pBufferInfo = &materialBufferInfo;
			descriptorWrites.push_back(materialWrite);

			VkDescriptorImageInfo shadowImageInfo{};
			shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			if (shadowMap) {
				shadowImageInfo.imageView = shadowMap->getShadowMapImageView();
				shadowImageInfo.sampler = shadowMap->getShadowSampler();
			} else {
				shadowImageInfo.imageView = textureImageView;
				shadowImageInfo.sampler = textureSampler;
			}

			VkWriteDescriptorSet shadowWrite{};
			shadowWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			shadowWrite.dstSet = obj.descriptorSets[matIndex][frame];
			shadowWrite.dstBinding = 3;
			shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			shadowWrite.descriptorCount = 1;
			shadowWrite.pImageInfo = &shadowImageInfo;
			descriptorWrites.push_back(shadowWrite);

			vkUpdateDescriptorSets(device->getDevice(),
				static_cast<uint32_t>(descriptorWrites.size()),
				descriptorWrites.data(), 0, nullptr);
		}
	}

	obj.loaded = true;
	std::cout << "Loaded object: " << sceneObj.name << " (" << sceneObj.modelPath << ")" << std::endl;
	return obj;
}

void VulkanApplication::destroyLoadedObject(LoadedObject& obj)
{
	if (!obj.loaded) return;

	for (size_t i = 0; i < obj.uniformBuffers.size(); i++) {
		if (obj.uniformBuffers[i] != VK_NULL_HANDLE) {
			vkDestroyBuffer(device->getDevice(), obj.uniformBuffers[i], nullptr);
		}
		if (obj.uniformBuffersMemory[i] != VK_NULL_HANDLE) {
			vkFreeMemory(device->getDevice(), obj.uniformBuffersMemory[i], nullptr);
		}
	}
	obj.uniformBuffers.clear();
	obj.uniformBuffersMemory.clear();
	obj.uniformBuffersMapped.clear();

	for (size_t matIndex = 0; matIndex < obj.materialUniformBuffers.size(); matIndex++) {
		for (size_t frame = 0; frame < obj.materialUniformBuffers[matIndex].size(); frame++) {
			if (obj.materialUniformBuffers[matIndex][frame] != VK_NULL_HANDLE) {
				vkDestroyBuffer(device->getDevice(), obj.materialUniformBuffers[matIndex][frame], nullptr);
			}
			if (obj.materialUniformBuffersMemory[matIndex][frame] != VK_NULL_HANDLE) {
				vkFreeMemory(device->getDevice(), obj.materialUniformBuffersMemory[matIndex][frame], nullptr);
			}
		}
	}
	obj.materialUniformBuffers.clear();
	obj.materialUniformBuffersMemory.clear();
	obj.materialUniformBuffersMapped.clear();

	obj.descriptorSets.clear();

	if (obj.model.vertexBuffer != VK_NULL_HANDLE) {
		objectLoader->destroyModel(obj.model);
	}

	obj.loaded = false;
}

void VulkanApplication::destroyAllLoadedObjects()
{
	if (physicsEngine) {
		for (auto& obj : loadedObjects) {
			if (obj.vehicle) {
				obj.vehicle->shutdown();
				obj.vehicle.reset();
			}
			if (obj.physicsBodyID != 0xFFFFFFFF) {
				physicsEngine->removeBody(obj.physicsBodyID);
				obj.physicsBodyID = 0xFFFFFFFF;
			}
		}
	}
	for (auto& obj : loadedObjects) {
		destroyLoadedObject(obj);
	}
	loadedObjects.clear();
}

void VulkanApplication::initPhysics()
{
	physicsEngine = std::make_unique<PhysicsEngine>();
	physicsEngine->init();

	// Ground plane for the car to drive on
	JPH::ShapeRefC groundShape = new JPH::BoxShape(JPH::Vec3(200.0f, 0.5f, 200.0f));
	physicsEngine->createStaticBody(glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(0.0f), groundShape);

	for (auto& obj : loadedObjects) {
		if (!obj.loaded || !obj.physics.enabled) continue;

		if (obj.physics.isVehicle) {
			JPH::ShapeRefC chassisShape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.2f, 1.0f));
			float mass = obj.physics.mass > 0.0f ? obj.physics.mass : 500.0f;
			JPH::Body* chassisBody = physicsEngine->createRigidBody(
				obj.transform.position,
				obj.transform.rotation,
				chassisShape,
				mass,
				true);
			if (chassisBody) {
				obj.physicsBodyID = chassisBody->GetID().GetIndexAndSequenceNumber();

				VehicleConfig vConfig;
				vConfig.wheels = {
					{{ 0.6f, -0.2f, 1.4f}, 0.3f, 0.2f, 0.3f, 200.0f, 20.0f, 3000.0f, true},
					{{-0.6f, -0.2f, 1.4f}, 0.3f, 0.2f, 0.3f, 200.0f, 20.0f, 3000.0f, true},
					{{ 0.6f, -0.2f,-1.4f}, 0.3f, 0.2f, 0.3f, 200.0f, 20.0f, 3000.0f, false},
					{{-0.6f, -0.2f,-1.4f}, 0.3f, 0.2f, 0.3f, 200.0f, 20.0f, 3000.0f, false},
				};

				auto vehicle = std::make_unique<VehiclePhysics>();
				vehicle->init(physicsEngine.get(), chassisBody, vConfig);
				obj.vehicle = std::move(vehicle);
				std::cout << "Created vehicle body for object" << std::endl;
			}
		}
		else if (obj.physics.isDynamic) {
			JPH::ShapeRefC shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.3f, 1.0f));
			JPH::Body* body = physicsEngine->createRigidBody(
				obj.transform.position,
				obj.transform.rotation,
				shape,
				obj.physics.mass,
				true);
			if (body) {
				obj.physicsBodyID = body->GetID().GetIndexAndSequenceNumber();
				std::cout << "Created dynamic body for object" << std::endl;
			}
		}
	}
}

void VulkanApplication::syncPhysicsTransforms()
{
	for (auto& obj : loadedObjects) {
		if (!obj.loaded || obj.physicsBodyID == 0xFFFFFFFF) continue;
		obj.transform.position = physicsEngine->getBodyPosition(obj.physicsBodyID);
		glm::quat q = physicsEngine->getBodyRotation(obj.physicsBodyID);
		glm::vec3 euler = glm::degrees(glm::eulerAngles(q));
		obj.transform.rotation = euler;
		if (obj.vehicle && obj.physicsBodyID != 0xFFFFFFFF) {
			float speed = obj.vehicle->getSpeed();
			float rpm = obj.vehicle->getRPM();
			int gear = obj.vehicle->getCurrentGear();
			static int frameCount = 0;
			frameCount++;
			if (frameCount % 60 == 0) {
				std::cout << "Car speed: " << speed << " m/s  RPM: " << rpm
					<< "  Gear: " << gear
					<< "  pos: (" << obj.transform.position.x << ", "
					<< obj.transform.position.y << ", "
					<< obj.transform.position.z << ")"
					<< "  input: T=" << vehicleThrottle << " B=" << vehicleBrake << " S=" << vehicleSteering
					<< std::endl;
			}
			if (vehicleThrottle > 0.0f || vehicleBrake > 0.0f) {
				JPH::BodyID id(obj.physicsBodyID);
				JPH::Vec3 fwd = physicsEngine->getBodyInterface().GetRotation(id) * JPH::Vec3(0, 0, 1);
				float forceMagnitude = vehicleThrottle > 0.0f ? 3000.0f : -1000.0f;
				physicsEngine->getBodyInterface().AddForce(id, fwd * forceMagnitude);
			}
			if (vehicleSteering != 0.0f) {
				JPH::BodyID id(obj.physicsBodyID);
				physicsEngine->getBodyInterface().AddTorque(id, JPH::Vec3(0, vehicleSteering * 200.0f, 0));
			}

		}
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
	cleanupRayTracingGeometryBuffers();
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
	LoadedObject* rtObj = nullptr;
	for (auto& obj : loadedObjects) {
		if (obj.loaded && obj.model.indexBuffer != VK_NULL_HANDLE && obj.model.rtVertexBuffer != VK_NULL_HANDLE) {
			rtObj = &obj;
			break;
		}
	}
	if (!rtObj) return;

	if (rayTracingPrimitiveBuffer == VK_NULL_HANDLE || rayTracingMeshBuffer == VK_NULL_HANDLE) {
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
	indexBufferInfo.buffer = rtObj->model.indexBuffer;
	indexBufferInfo.offset = 0;
	indexBufferInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo vertexBufferInfo{};
	vertexBufferInfo.buffer = rtObj->model.rtVertexBuffer;
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
	for (size_t i = 0; i < rtObj->model.textures.size() && i < 16; i++) {
		if (rtObj->model.textures[i].imageView != VK_NULL_HANDLE) {
			texImageInfos[i].imageView = rtObj->model.textures[i].imageView;
			texImageInfos[i].sampler = rtObj->model.textures[i].sampler;
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
	//ShadowMap Sampler (binding = 3)
	VkDescriptorSetLayoutBinding shadowLayoutBinding{};
	shadowLayoutBinding.binding = 3;
	shadowLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowLayoutBinding.descriptorCount = 1;
	shadowLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	std::array<VkDescriptorSetLayoutBinding, 4> bindings = {
		uboLayoutBinding,
		samplerLayoutBinding,
		materialLayoutBinding,
		shadowLayoutBinding
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
