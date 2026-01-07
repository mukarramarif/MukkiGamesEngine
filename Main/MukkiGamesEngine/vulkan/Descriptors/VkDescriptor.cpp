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
	std::array<VkDescriptorPoolSize, 3> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = maxSets;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = maxSets;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[2].descriptorCount = maxSets;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
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
}

void VkDescriptorBoss::updateDescriptorSets(const std::vector<VkDescriptorSet>& descriptorSets, const std::vector<VkBuffer>& uniformBuffers, VkImageView textureImageView, VkSampler textureSampler)
{
	for (size_t i = 0; i < descriptorSets.size(); i++) {
		// Update uniform buffer (binding = 0)
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(uniformBufferObject);

		VkWriteDescriptorSet uboWrite{};
		uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uboWrite.dstSet = descriptorSets[i];
		uboWrite.dstBinding = 0; // Uniform buffer binding
		uboWrite.dstArrayElement = 0;
		uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboWrite.descriptorCount = 1;
		uboWrite.pBufferInfo = &bufferInfo;

		// Update texture sampler (binding = 1)
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = textureImageView;
		imageInfo.sampler = textureSampler;

		VkWriteDescriptorSet samplerWrite{};
		samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		samplerWrite.dstSet = descriptorSets[i];
		samplerWrite.dstBinding = 1; // Texture sampler binding
		samplerWrite.dstArrayElement = 0;
		samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerWrite.descriptorCount = 1;
		samplerWrite.pImageInfo = &imageInfo;

		// Update both descriptors
		// @TO-DO: Add sample writer for textures
		std::array<VkWriteDescriptorSet, 1> descriptorWrites = { uboWrite }; 
		vkUpdateDescriptorSets(device->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
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
