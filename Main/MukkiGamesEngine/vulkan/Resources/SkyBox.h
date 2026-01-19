#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "../Core/VkDevice.h"
#include "../pipeline.h"
#include "TextureManager.h"
#include "BufferManager.h"

// Single combined VP matrix for skybox shader
struct SkyboxUBO {
	glm::mat4 VP;
};

class SkyBox
{
public:
	SkyBox();
	~SkyBox();

	void init(Device* device, TextureManager* textureManager, BufferManager* bufferManager,
		VkRenderPass renderPass, const std::string& cubemapPath,
		CubemapLayout layout, uint32_t maxFramesInFlight);

	void updateUniformBuffer(uint32_t currentFrame, const glm::mat4& view);
	void s_recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t currentFrame);
	void cleanup();

private:
	void createUniformBuffers();
	void createDescriptorSetLayout();
	void createDescriptorPool();
	void createDescriptorSets();
	
	void createPipelineLayout();
	void createPipeline(VkRenderPass renderPass);

	Device* device = nullptr;
	TextureManager* textureManager = nullptr;
	BufferManager* bufferManager = nullptr;

	

	// Cubemap texture
	VkImage cubemapImage = VK_NULL_HANDLE;
	VkDeviceMemory cubemapImageMemory = VK_NULL_HANDLE;
	VkImageView cubemapImageView = VK_NULL_HANDLE;
	VkSampler cubemapSampler = VK_NULL_HANDLE;

	// Uniform buffers	
	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;

	// Descriptors
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets;

	// Pipeline
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VulkanPipeline* skyboxPipeline = nullptr;

	uint32_t maxFramesInFlight = 2;
};
