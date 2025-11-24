#include "VkDescriptor.h"
#include <stdexcept>
#include <array>

VkDescriptorBoss::VkDescriptorBoss(const Device* device, uint32_t maxSets)
	: device(device), descriptorPool(VK_NULL_HANDLE)
{
	createDescriptorPool(maxSets);
}

void VkDescriptorBoss::createDescriptorPool(uint32_t maxSets)
{
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = maxSets;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = maxSets;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;  // Fixed typo: Stype -> sType
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = maxSets;
	
	if (vkCreateDescriptorPool(device->getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void VkDescriptorBoss::createDescriptorSets(VkDescriptorSetLayout descriptorSetLayout, uint32_t count, std::vector<VkDescriptorSet>& descriptorSets)
{
	std::vector<VkDescriptorSetLayout> layouts(count, descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(count);
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(count);
	if (vkAllocateDescriptorSets(device->getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	// Note: Descriptor set updates would go here when you have uniform buffers, textures, etc.
	// For now, we just allocate the sets
}

void VkDescriptorBoss::destroyDescriptorPool()
{
	if (descriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device->getDevice(), descriptorPool, nullptr);
		descriptorPool = VK_NULL_HANDLE;
	}
}

void VkDescriptorBoss::cleanup()
{
	destroyDescriptorPool();
}

VkDescriptorBoss::~VkDescriptorBoss()
{
	cleanup();
}
