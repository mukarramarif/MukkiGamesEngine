#pragma once
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
#include "../uiManager/uiManager.h"
#include "../pipeline/ComputePipeline.h"	
#include "../objects/lights.h"
#include <vector>
#include "ShaderCompiler.h"

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
		COMPUTE
	};
	RenderMode currentRenderMode = RenderMode::GRAPHICS;
	// Core components
	Instance instance;
	EngineWindow* window = nullptr;
	Device* device = nullptr;
	VulkanSwap* swapChain = nullptr;
	
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
	void updateUniformBuffer(uint32_t currentImage);
	void createTextureResources();
	void drawFrame();
	void recreateSwapChain();
	void SetupUIManager();
	void initComputePipeline();
	void createComputeOutputImage();
	void recordComputeCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void loadModel(const std::string& filepath);
	void setupDefaultLights();
	void cleanupComputeResources();
	
	// New methods for pipeline setup
	void createDescriptorSetLayout();
	void createPipelineLayout();
	void createGraphicsPipeline();
	
	// Input handling methods
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

	//Object-Loader
	std::vector<std::vector<VkDescriptorSet>> modelDescriptorSets; // set for each model and each frame
	ObjectLoader* objectLoader = nullptr;
	Model loadedModel;
	bool modelLoaded = false;
	void createModelDescriptorSets();

	//Skybox
	SkyBox* skybox = nullptr;

	std::vector<Light> lights;
	float ambientStrength = 0.1f;
};