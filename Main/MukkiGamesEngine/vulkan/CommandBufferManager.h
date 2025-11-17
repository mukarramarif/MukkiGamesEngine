#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "Core/VkDevice.h"

class CommandBufferManager {
public:
	CommandBufferManager();
	~CommandBufferManager();

	void init(const Device& device, uint32_t maxFramesInFlight);
	void cleanup();

	VkCommandPool getCommandPool() const { return commandPool };
private:
	const Device* device;
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;

	void createCommandPool();
	void createCommandBuffers(uint32_t maxFramesInFlight);
};