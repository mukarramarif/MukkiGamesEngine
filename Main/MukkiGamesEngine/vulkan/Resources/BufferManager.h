#pragma once

#include "<vulkan/vulkan.h>"
#include <vector>
#include "../Core/VkDevice.h"
#include "../CommandBufferManager.h"
#include "../objects/vertex.h"

class BufferManager {
public:
	BufferManager();
	~BufferManager();
	void init(const VkDevice& device, const CommandBufferManager& commandBufferManager);
	void cleanup();
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
		VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void createVertexBuffer(const std::vector<Vertex>& vertices, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory);
	void indexBuffer(const std::vector<uint32_t>& indices, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory);
	void createUniformBuffer(VkDeviceSize size, VkBuffer& uniformBuffer, VkDeviceMemory& uniformBufferMemory);
	void destroyBuffer(VkBuffer buffer, VkDeviceMemory bufferMemory);
private:
	const VkDevice* device;
	const CommandBufferManager* commandBufferManager;
};