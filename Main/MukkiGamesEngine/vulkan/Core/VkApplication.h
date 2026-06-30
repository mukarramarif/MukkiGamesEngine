#pragma once
#include <memory>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include "VkInstance.h"
#include "VkDevice.h"
#include "EngineWindow.h"
#include "SwapChain.h"
#include "../pipeline.h"
#include "../RenderPass.h"
#include "../CommandBufferManager.h"
#include "../Descriptors/VkDescriptor.h"
#include "../Resources/TextureManager.h"
#include "../Resources/BufferManager.h"
#include "../Resources/Camera.h"
#include "../Resources/ObjectLoader.h"
#include "../Resources/SkyBox.h"
#include "../Resources/Sceneloader.h"
#include "../Resources/DeletionQueue.h"
#include "../uiManager/uiManager.h"
#include "../pipeline/computePipeline.h"
#include "../objects/lights.h"
#include <vector>
#include <string>
#include "ShaderCompiler.h"
#include "../raytracing/RayTracingAS.h"
#include "../raytracing/RayTracingPipeline.h"

const int MAX_FRAMES_IN_FLIGHT = 2;

class VulkanApplication {
public:
	// Explicit constructor and destructor
	VulkanApplication();
	~VulkanApplication();
	void run();
	void toggleRenderMode();
private:
	// Render Mode toggle
	vk::raii::Context context;
	enum class RenderMode {
		GRAPHICS,
     COMPUTE,
		RAYTRACING
	};
	RenderMode currentRenderMode = RenderMode::GRAPHICS;
	// Core components
	Instance instance;
	std::unique_ptr<EngineWindow> window;
	std::unique_ptr<Device> device;
	std::unique_ptr<VulkanSwap> swapChain;

	// Rendering components
	VulkanRenderPass* renderPassObj = nullptr;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VulkanPipeline* graphicsPipeline = nullptr;
	VulkanPipeline* additivePipeline = nullptr;
	// Pipeline layout and descriptor set layout (now managed here)
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

	// Command buffers
	CommandBufferManager* commandBufferManager = nullptr;

	// Synchronization objects
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	std::vector<VkFence> imagesInFlight;
  std::vector<VkImageLayout> swapChainImageLayouts;
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
	VkDescriptorSetLayout rayTracingDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool rayTracingDescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet rayTracingDescriptorSet = VK_NULL_HANDLE;
	VkBuffer rayTracingUniformBuffer = VK_NULL_HANDLE;
	VkDeviceMemory rayTracingUniformBufferMemory = VK_NULL_HANDLE;
	void* rayTracingUniformBufferMapped = nullptr;

	struct RayTracingPrimitiveInfo {
		uint32_t firstIndex;
		uint32_t indexCount;
		int32_t textureIndex;
		float metallicFactor;
		float roughnessFactor;
		float baseColorR;
		float baseColorG;
		float baseColorB;
	};
	struct RayTracingMeshInfo {
		uint32_t primitiveOffset;
		uint32_t primitiveCount;
		uint32_t pad0;
		uint32_t pad1;
	};
	VkBuffer rayTracingPrimitiveBuffer = VK_NULL_HANDLE;
	VkDeviceMemory rayTracingPrimitiveBufferMemory = VK_NULL_HANDLE;
	VkBuffer rayTracingMeshBuffer = VK_NULL_HANDLE;
	VkDeviceMemory rayTracingMeshBufferMemory = VK_NULL_HANDLE;

	//Texture Handler and Buffer Manager
	TextureManager* textureManager = nullptr;
	BufferManager* bufferManager = nullptr;
	//depth resources
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;
	//Texture resources
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;
	//Uniform Buffers
	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;

	std::vector<VkBuffer> defaultMaterialUniformBuffers;
	std::vector<VkDeviceMemory> defaultMaterialUniformBuffersMemory;
	std::vector<void*> defaultMaterialUniformBuffersMapped;

	std::vector<std::vector<VkBuffer>> materialUniformBuffers;
	std::vector<std::vector<VkDeviceMemory>> materialUniformBuffersMemory;
	std::vector<std::vector<void*>> materialUniformBuffersMapped;

	// Camera
	Camera* camera = nullptr;

	// Input tracking
	float lastX = 400.0f;
	float lastY = 300.0f;
	bool firstMouse = true;
	float deltaTime = 0.0f;
	float lastFrame = 0.0f;
	bool cursorEnabled = false;
	// Methods
	void initVulkan();
	void mainLoop();
	void cleanup();
	void toggleCursor();
	void createSyncObjects();
	void createVertexBuffer();
	void createIndexBuffer();
	void createUniformBuffers();
	void createDefaultMaterialUniformBuffers();
	void createMaterialUniformBuffers();
	void cleanupMaterialUniformBuffers();
    void createRayTracingUniformBuffer();
	void updateUniformBuffer(uint32_t currentImage);
    void updateRayTracingUniformBuffer();
	void createTextureResources();
	void drawFrame();
	void recreateSwapChain();
	void SetupUIManager();
	void initComputePipeline();
    void initRayTracingPipeline();
	void createComputeOutputImage();
	void createRayTracingGeometryBuffers();
	void cleanupRayTracingGeometryBuffers();
	void recordComputeCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void recordRayTracingCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void loadModel(const std::string& filepath);
	void setupDefaultLights();
	void cleanupComputeResources();
	void createTAAPipeline();
	void createTAAImages();
	void cleanupTAAImages();
	void cleanupTAAPipeline();
	void updateTAADescriptorSets();

	// New methods for pipeline setup
	void createDescriptorSetLayout();
    void createRayTracingDescriptorSetLayout();
	void createRayTracingDescriptorPool();
	void createRayTracingDescriptorSet();
	void createPipelineLayout();
	void createGraphicsPipeline();

	// Input handling methods
    void createAccumulationImage();
    void cleanupAccumulationResources();
	void processInput();
	static void mouseCallback(GLFWwindow* window, double xpos, double ypos);

	//UI Manager
	UIManager* uiManager = nullptr;
	ModelTransform modelTransform;
	// Compute Pipeline
	ComputePipeline* computePipeline = nullptr;
	VkImage computeOutputImage = VK_NULL_HANDLE;
	VkDeviceMemory computeOutputImageMemory = VK_NULL_HANDLE;
	VkImageView computeOutputImageView = VK_NULL_HANDLE;
	VkExtent2D computeOutputImageExtent{};

	// TAA resources
	VkPipeline taaPipeline = VK_NULL_HANDLE;
	VkPipelineLayout taaPipelineLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout taaDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool taaDescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet taaDescriptorSet = VK_NULL_HANDLE;
	VkImage taaHistoryImage = VK_NULL_HANDLE;
	VkDeviceMemory taaHistoryImageMemory = VK_NULL_HANDLE;
	VkImageView taaHistoryImageView = VK_NULL_HANDLE;
	VkImage taaOutputImage = VK_NULL_HANDLE;
	VkDeviceMemory taaOutputImageMemory = VK_NULL_HANDLE;
	VkImageView taaOutputImageView = VK_NULL_HANDLE;
	VkExtent2D taaImageExtent{};
	bool taaFirstFrame = true;
	uint64_t rtFrameCounter = 0;
	glm::vec2 taaJitter = glm::vec2(0.0f);

	RayTracingPipeline* rayTracingPipeline = nullptr;

	// Accumulation
	VkImage accumOutputImage = VK_NULL_HANDLE;
	VkDeviceMemory accumOutputImageMemory = VK_NULL_HANDLE;
	VkImageView accumOutputImageView = VK_NULL_HANDLE;
	uint32_t accumulationFrameCount = 0;
	bool cameraMoved = false;

	//Object-Loader
	std::vector<std::vector<VkDescriptorSet>> modelDescriptorSets; // set for each model and each frame
	ObjectLoader* objectLoader = nullptr;
	Model loadedModel;
	bool modelLoaded = false;
	void createModelDescriptorSets();

	//Scene Loader
	SceneLoader* sceneLoader = nullptr;

	//Skybox
	SkyBox* skybox = nullptr;
	RayTracingAS* rayTracingAS = nullptr;

	std::vector<Light> lights;
	float ambientStrength = 0.1f;
	// @TODO: find a way to automatically update scenes like hot shader reloading
	std::vector<std::string> availableScenes{ "WaterExample.json", "sceneTrack.json", "scene.json" };
	int currentSceneIndex = 0;

	// Deletion queue for deferred resource cleanup
	DeletionQueue m_deletionQueue;
};
