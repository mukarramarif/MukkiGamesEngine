#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include "Core/VkDevice.h"
class UIManager;
class SkyBox;
struct Model;
class CommandBufferManager {
public:
	CommandBufferManager();
	~CommandBufferManager();

	void init( Device* device, uint32_t maxFramesInFlight);
	void cleanup();

	VkCommandPool getCommandPool() const { return commandPool; };
	VkCommandBuffer getCommandBuffer(uint32_t index) const { return commandBuffers[index]; }
	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commmandBuffer);
	void resetCommandBuffer(uint32_t frameIndex);

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex,
		VkRenderPass renderPass, VkFramebuffer framebuffer,
		VkExtent2D extent, VkPipeline graphicsPipeline,
		VkPipelineLayout pipelineLayout, VkBuffer vertexBuffer,
        VkBuffer indexBuffer, VkImage swapChainImage, VkImageLayout swapChainOldLayout, const std::vector<VkDescriptorSet>& descriptorSets,
		uint32_t currentFrame, uint32_t indexCount, UIManager& uiManager);

	void beginModelRenderPass(
		VkCommandBuffer commandBuffer,
		VkRenderPass renderPass,
		VkFramebuffer framebuffer,
		VkExtent2D extent,
		VkPipeline graphicsPipeline,
		VkPipelineLayout pipelineLayout,
		SkyBox* skybox,
		VkImage swapChainImage,
		VkImageLayout swapChainOldLayout,
		uint32_t currentFrame);

	void recordModelDrawCommands(
		VkCommandBuffer commandBuffer,
		const Model& model,
		VkPipelineLayout pipelineLayout,
		VkPipeline graphicsPipeline,
		VkPipeline transparentPipeline,
		VkPipeline additivePipeline,
		const glm::vec3& cameraPosition,
		const std::vector<std::vector<VkDescriptorSet>>& materialDescriptorSets,
		uint32_t currentFrame);

	void endModelRenderPass(
		VkCommandBuffer commandBuffer);
private:
	Device* device;
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	
	void createCommandPool();
	void createCommandBuffers(uint32_t maxFramesInFlight);
};