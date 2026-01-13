#include "CommandBufferManager.h"
#include "uiManager/uiManager.h"
#include "Resources/ObjectLoader.h"
#include <stdexcept>
#include <array>
#include <vector>
#include <iostream> // For diagnostics

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
	uint32_t indexCount,
	UIManager& uiManager)
{
	static bool firstCall = true;
	if (firstCall) {
		std::cout << "\n=== COMMAND BUFFER RECORDING DIAGNOSTICS ===" << std::endl;
		std::cout << "Extent: " << extent.width << "x" << extent.height << std::endl;
		std::cout << "Index Count: " << indexCount << std::endl;
		std::cout << "Vertex Buffer: " << vertexBuffer << std::endl;
		std::cout << "Index Buffer: " << indexBuffer << std::endl;
		std::cout << "Pipeline: " << graphicsPipeline << std::endl;
		firstCall = false;
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

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

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(extent.width);
	viewport.height = static_cast<float>(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	
	/*std::cout << "Viewport set: " << viewport.width << "x" << viewport.height << std::endl;*/

	// Set scissor
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = extent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	// Bind vertex buffer
	VkBuffer vertexBuffers[] = { vertexBuffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

	// Bind index buffer
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	// Bind descriptor sets
	if (!descriptorSets.empty()) {
		vkCmdBindDescriptorSets(
			commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			0,
			1,
			&descriptorSets[currentFrame],
			0,
			nullptr
		);
	}

	// DRAW
	/*std::cout << "Drawing " << indexCount << " indices" << std::endl;*/
	vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
	uiManager.render(commandBuffer);
	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
}

void CommandBufferManager::recordModelCommandBuffer(
	VkCommandBuffer commandBuffer,
	uint32_t imageIndex,
	VkRenderPass renderPass,
	VkFramebuffer framebuffer,
	VkExtent2D extent,
	VkPipeline graphicsPipeline,
	VkPipeline additivePipeline,
	VkPipelineLayout pipelineLayout,
	const Model& model,
	const std::vector<std::vector<VkDescriptorSet>>& materialDescriptorSets,
	uint32_t currentFrame,
	UIManager& uiManager)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

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
	clearValues[0].color = { {0.1f, 0.1f, 0.1f, 1.0f} };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	// Set viewport
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(extent.width);
	viewport.height = static_cast<float>(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	// Set scissor
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = extent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	// Bind model buffers
	VkBuffer vertexBuffers[] = { model.vertexBuffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, model.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	VkPipeline currentPipeline = VK_NULL_HANDLE;
	// Render each mesh
	// First pass: Render opaque and standard alpha blended meshes
	for (const auto& mesh : model.meshes) {
		for (const auto& primitive : mesh.primitives) {
			int32_t matIndex = primitive.materialIndex >= 0 ? primitive.materialIndex : 0;

			// Skip emissive materials in first pass
			if (matIndex < static_cast<int32_t>(model.materials.size()) &&
				model.materials[matIndex].isEmissive) {
				continue;
			}

			// Bind standard pipeline if needed
			if (currentPipeline != graphicsPipeline) {
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
				currentPipeline = graphicsPipeline;
			}

			if (matIndex < static_cast<int32_t>(materialDescriptorSets.size())) {
				vkCmdBindDescriptorSets(
					commandBuffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					pipelineLayout,
					0,
					1,
					&materialDescriptorSets[matIndex][currentFrame],
					0,
					nullptr
				);
			}

			vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
		}
	}

	// Second pass: Render emissive/light flare meshes with additive blending
	for (const auto& mesh : model.meshes) {
		for (const auto& primitive : mesh.primitives) {
			int32_t matIndex = primitive.materialIndex >= 0 ? primitive.materialIndex : 0;

			// Only render emissive materials in second pass
			if (matIndex >= static_cast<int32_t>(model.materials.size()) ||
				!model.materials[matIndex].isEmissive) {
				continue;
			}

			// Bind additive pipeline if needed
			if (currentPipeline != additivePipeline) {
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, additivePipeline);
				currentPipeline = additivePipeline;
			}

			if (matIndex < static_cast<int32_t>(materialDescriptorSets.size())) {
				vkCmdBindDescriptorSets(
					commandBuffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					pipelineLayout,
					0,
					1,
					&materialDescriptorSets[matIndex][currentFrame],
					0,
					nullptr
				);
			}

			vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
		}
	}

	uiManager.render(commandBuffer);
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
	}
}
VkCommandBuffer CommandBufferManager::beginSingleTimeCommands() {
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
