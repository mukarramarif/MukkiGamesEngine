#pragma once
#include "../Core/VkDevice.h"
#include "../pipeline.h"
#include "TextureManager.h"
#include "BufferManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct SkyboxUBO {
	glm::mat4 view;
	glm::mat4 projection;
};

class SkyBox {
public:
	SkyBox();
	~SkyBox();

	void init(Device* device, TextureManager* textureManager, BufferManager* bufferManager,
		VkRenderPass renderPass, const std::string& cubemapPath,
		CubemapLayout layout = CubemapLayout::HorizontalCross, uint32_t maxFramesInFlight = 2);

	void cleanup();
	void s_recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t currentFrame);
	void updateUniformBuffer(uint32_t currentFrame, const glm::mat4& view, const glm::mat4& projection);

private:
	void createDescriptorSetLayout();
	void createPipelineLayout();
	void createPipeline(VkRenderPass renderPass);
	void createVertexBuffer();
	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();

	Device* device = nullptr;
	TextureManager* textureManager = nullptr;
	BufferManager* bufferManager = nullptr;

	// Pipeline
	VulkanPipeline* skyboxPipeline = nullptr;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

	// Cubemap texture
	VkImage cubemapImage = VK_NULL_HANDLE;
	VkDeviceMemory cubemapImageMemory = VK_NULL_HANDLE;
	VkImageView cubemapImageView = VK_NULL_HANDLE;
	VkSampler cubemapSampler = VK_NULL_HANDLE;

	// Vertex buffer (cube)
	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
	uint32_t vertexCount = 0;

	// Uniform buffers
	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;

	// Descriptors
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets;

	uint32_t maxFramesInFlight = 2;
};
