#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "Core/VkDevice.h"
class UIManager;
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
		VkBuffer indexBuffer, const std::vector<VkDescriptorSet>& descriptorSets,
		uint32_t currentFrame, uint32_t indexCount, UIManager& uiManager);
	void recordModelCommandBuffer(
		VkCommandBuffer commandBuffer,
		uint32_t imageIndex,
		VkRenderPass renderPass,
		VkFramebuffer framebuffer,
		VkExtent2D extent,
		VkPipeline graphicsPipeline,
		VkPipelineLayout pipelineLayout,
		const Model& model,
		const std::vector<std::vector<VkDescriptorSet>>& materialDescriptorSets,
		uint32_t currentFrame,
		UIManager& uiManager);
private:
	Device* device;
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	
	void createCommandPool();
	void createCommandBuffers(uint32_t maxFramesInFlight);
};