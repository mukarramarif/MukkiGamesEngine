#include "CommandBufferManager.h"
#include <stdexcept>
#include <array>
#include <vector>

CommandBufferManager::CommandBufferManager()
	: device(nullptr), commandPool(VK_NULL_HANDLE)
{
	// Don't call init here - device is not set yet
}

CommandBufferManager::~CommandBufferManager()
{
	cleanup();
}

void CommandBufferManager::cleanup()
{
	if (device && commandPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(device->getDevice(), commandPool, nullptr);
		commandPool = VK_NULL_HANDLE;
	}
}

void CommandBufferManager::init(Device* device, uint32_t maxFramesInFlight)
{
	this->device = device;
	createCommandPool();
	createCommandBuffers(maxFramesInFlight);
}

VkCommandBuffer CommandBufferManager::beginSingleTimeCommands()
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device->getDevice(), &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void CommandBufferManager::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(device->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(device->getGraphicsQueue());

	vkFreeCommandBuffers(device->getDevice(), commandPool, 1, &commandBuffer);
}

void CommandBufferManager::resetCommandBuffer(uint32_t frameIndex)
{
	vkResetCommandBuffer(commandBuffers[frameIndex], 0);
}

void CommandBufferManager::recordCommandBuffer(
	VkCommandBuffer commandBuffer,
	uint32_t imageIndex,
	VkRenderPass renderPass,
	VkFramebuffer framebuffer,
	VkExtent2D extent,
	VkPipeline graphicsPipeline,
	VkPipelineLayout pipelineLayout,
	VkBuffer vertexBuffer,
	VkBuffer indexBuffer,
	const std::vector<VkDescriptorSet>& descriptorSets,
	uint32_t currentFrame,
	uint32_t indexCount)
{
	// Begin command buffer
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	// Setup render pass
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = framebuffer;
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = extent;

	// Setup clear values
	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };  // Black background
	clearValues[1].depthStencil = { 1.0f, 0 };

	// FIX: Don't try to set renderPassInfo members that don't exist
	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	// Begin render pass
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	// Bind pipeline
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	// Bind vertex buffer
	VkBuffer vertexBuffers[] = { vertexBuffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

	// FIX: Use correct index type - uint16_t uses VK_INDEX_TYPE_UINT16
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

	// Bind descriptor sets (if any)
	if (!descriptorSets.empty()) {
		vkCmdBindDescriptorSets(
			commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			0,
			1,  // Bind only one descriptor set per frame
			&descriptorSets[currentFrame],
			0,
			nullptr
		);
	}

	// Set dynamic viewport
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(extent.width);
	viewport.height = static_cast<float>(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	// Set dynamic scissor
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = extent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	// Draw indexed
	vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

	// End render pass
	vkCmdEndRenderPass(commandBuffer);

	// End command buffer
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
	}
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
