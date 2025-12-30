#include "BufferManager.h"
#include <stdexcept>

BufferManager::BufferManager() : device(nullptr), commandBufferManager(nullptr) {}
BufferManager::~BufferManager() {
	cleanup();
}
void BufferManager::cleanup() {
	// No dynamic resources to clean up in this manager itself

}
void BufferManager::init( Device& device,CommandBufferManager& commandBufferManager) {
	this->device = &device;
	this->commandBufferManager = &commandBufferManager;
}
void BufferManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
	VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
	// Implementation of buffer creation
	VkBufferCreateInfo  createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.size = size;
	createInfo.usage = usage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if(vkCreateBuffer(device->getDevice(), &createInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer!");
	}
	VkMemoryRequirements memoryRq;
	vkGetBufferMemoryRequirements(device->getDevice(), buffer, &memoryRq);
	
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memoryRq.size;
	allocInfo.memoryTypeIndex = device->findMemoryType(memoryRq.memoryTypeBits, properties);
	if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate buffer memory!");
	}
	vkBindBufferMemory(device->getDevice(), buffer, bufferMemory, 0);
}
void BufferManager::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
	// Implementation of buffer copy
	// getting commandbuffer instance
	VkCommandBuffer commandBuffer = commandBufferManager->beginSingleTimeCommands();
	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, & copyRegion);
	//release commandBuffer
	commandBufferManager->endSingleTimeCommands(commandBuffer);

}
void BufferManager::createVertexBuffer(const std::vector<Vertex>& vertices, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory) {
	// Implementation of vertex buffer creation
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
	VkDeviceMemory stagingBufferMemory;
	VkBuffer stagingBuffer;
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);
	void* data;
	vkMapMemory(device->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vkUnmapMemory(device->getDevice(), stagingBufferMemory);
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
	copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
	vkDestroyBuffer(device->getDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device->getDevice(), stagingBufferMemory, nullptr);
}
void BufferManager::indexBuffer(const std::vector<uint32_t>& indices, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory) {
	// Implementation of index buffer creation
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
	VkDeviceMemory stagingBufferMemory;
	VkBuffer stagingBuffer;
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);
	void* data;
	vkMapMemory(device->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(device->getDevice(), stagingBufferMemory);
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
	copyBuffer(stagingBuffer, indexBuffer, bufferSize);
	vkDestroyBuffer(device->getDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device->getDevice(), stagingBufferMemory, nullptr);

}
// @TODO: Implement createUniformBuffer
void BufferManager::createUniformBuffer(VkDeviceSize size, VkBuffer& uniformBuffer, VkDeviceMemory& uniformBufferMemory) {
	// Implementation of uniform buffer creation
	VkDeviceSize bufferSize = size;
}
void BufferManager::destroyBuffer(VkBuffer buffer, VkDeviceMemory bufferMemory) {
	vkDestroyBuffer(device->getDevice(), buffer, nullptr);
	vkFreeMemory(device->getDevice(), bufferMemory, nullptr);
}
