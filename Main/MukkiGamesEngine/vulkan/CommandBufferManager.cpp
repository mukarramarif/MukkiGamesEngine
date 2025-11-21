#include "CommandBufferManager.h"

CommandBufferManager::CommandBufferManager()
{
	init(device, 2);
}

CommandBufferManager::~CommandBufferManager()
{
	cleanup();
}
void CommandBufferManager::cleanup()
{
	if (vulkanDevice && commandPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(vulkanDevice->getLogicalDevice(), commandPool, nullptr);
		commandPool = VK_NULL_HANDLE;
	}
}
void CommandBufferManager::init(const Device& device, uint32_t maxFramesInFlight)
{
	this->device = &device;
	createCommandPool();
	createCommandBuffers(maxFramesInFlight);
}

VkCommandBuffer CommandBufferManager::beginSingleTimeCommands()
{
	return VkCommandBuffer();
}

void CommandBufferManager::endSingleTimeCommands(VkCommandBuffer commmandBuffer)
{
}

void CommandBufferManager::resetCommandBuffer(uint32_t frameIndex)
{
}

void CommandBufferManager::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, VkRenderPass renderPass, VkFramebuffer framebuffer, VkExtent2D extent, VkPipeline graphicsPipeline, VkPipelineLayout pipelineLayout, VkBuffer vertexBuffer, VkBuffer indexBuffer, const std::vector<VkDescriptorSet>& descriptorSets, uint32_t currentFrame, uint32_t indexCount)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = framebuffer;
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = extent;
	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	clearValues[1].depthStencil = { 1.0f, 0 };
	renderPass.Info.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
	VkBuffer vertexBuffers[] = { vertexBuffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
		static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(extent.width);
	viewport.height = static_cast<float>(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = extent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
	vkCmdEndRenderPass(commandBuffer);
	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}

}
void CommandBufferManager::createCommandBuffers(uint32_t maxFramesInFlight)
{
	commandBuffers.resize(maxFramesInFlight);
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
	if (vkAllocateCommandBuffers(device->getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}x
}
void CommandBufferManager::createCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = device->findQueueFamilies(device->getPhysicalDevice());
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
	if (vkCreateCommandPool(device->getDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool!");
	}

}
