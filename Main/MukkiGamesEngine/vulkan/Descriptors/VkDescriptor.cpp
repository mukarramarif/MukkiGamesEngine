#include "VkDescriptor.h"
#include "../objects/UBO.h"
#include "../Resources/ProbeVolume.h"
#include <stdexcept>
#include <array>

VkDescriptorBoss::VkDescriptorBoss(const Device* device, uint32_t maxSets)
	: device(device), descriptorPool(VK_NULL_HANDLE)
{
	createDescriptorPool(maxSets);
}

void VkDescriptorBoss::createDescriptorPool(uint32_t maxSets)
{
	uint32_t totalSets = maxSets * 500;
	std::array<VkDescriptorPoolSize, 3> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = totalSets * 3;  // UBO + MaterialUBO + probe params
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = totalSets * 6;  // base + shadows + probes + skybox
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[2].descriptorCount = totalSets;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = totalSets;

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

void VkDescriptorBoss::updateDescriptorSets(
	const std::vector<VkDescriptorSet>& descriptorSets,
	const std::vector<VkBuffer>& uniformBuffers,
	const std::vector<VkBuffer>& materialBuffers,
	VkImageView textureImageView,
	VkSampler textureSampler,
	VkImageView shadowMapImageView,
	VkSampler shadowMapSampler,
    VkImageView cubeShadowMapImageView,
    VkSampler cubeShadowMapSampler,
    VkImageView probeIrradianceView,
    VkSampler probeIrradianceSampler,
    VkImageView probeDepthView,
    VkSampler probeDepthSampler,
    VkBuffer probeParamsBuffer,
    VkImageView skyboxView,
    VkSampler skyboxSampler)
{
	for (size_t i = 0; i < descriptorSets.size(); i++) {
		std::vector<VkWriteDescriptorSet> descriptorWrites;
		// Update uniform buffer (binding = 0)
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkWriteDescriptorSet uboWrite{};
		uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uboWrite.dstSet = descriptorSets[i];
		uboWrite.dstBinding = 0; // Uniform buffer binding
		uboWrite.dstArrayElement = 0;
		uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboWrite.descriptorCount = 1;
		uboWrite.pBufferInfo = &bufferInfo;
		descriptorWrites.push_back(uboWrite);
		// Material buffer (binding = 2)
		VkDescriptorBufferInfo materialInfo{};
		materialInfo.buffer = materialBuffers[i];
		materialInfo.offset = 0;
		materialInfo.range = sizeof(MaterialUBO);

		VkWriteDescriptorSet materialWrite{};
		materialWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		materialWrite.dstSet = descriptorSets[i];
		materialWrite.dstBinding = 2;
		materialWrite.dstArrayElement = 0;
		materialWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		materialWrite.descriptorCount = 1;
		materialWrite.pBufferInfo = &materialInfo;
		descriptorWrites.push_back(materialWrite);

		VkDescriptorImageInfo imageInfo{};
		// Update texture sampler (binding = 1)
		if (textureImageView != VK_NULL_HANDLE && textureSampler != VK_NULL_HANDLE) {
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
			descriptorWrites.push_back(samplerWrite);
		}

		// Shadow map sampler (binding = 3)
		if (shadowMapImageView != VK_NULL_HANDLE && shadowMapSampler != VK_NULL_HANDLE) {
			VkDescriptorImageInfo shadowImageInfo{};
			shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			shadowImageInfo.imageView = shadowMapImageView;
			shadowImageInfo.sampler = shadowMapSampler;

			VkWriteDescriptorSet shadowWrite{};
			shadowWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			shadowWrite.dstSet = descriptorSets[i];
			shadowWrite.dstBinding = 3; // Shadow map sampler binding
			shadowWrite.dstArrayElement = 0;
			shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			shadowWrite.descriptorCount = 1;
			shadowWrite.pImageInfo = &shadowImageInfo;
			descriptorWrites.push_back(shadowWrite);
		}
		if (cubeShadowMapImageView != VK_NULL_HANDLE && cubeShadowMapSampler != VK_NULL_HANDLE) {
            VkDescriptorImageInfo cubeShadowInfo{};
            cubeShadowInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            cubeShadowInfo.imageView = cubeShadowMapImageView;
            cubeShadowInfo.sampler = cubeShadowMapSampler;

            VkWriteDescriptorSet cubeShadowWrite{};
            cubeShadowWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            cubeShadowWrite.dstSet = descriptorSets[i];
            cubeShadowWrite.dstBinding = 4;
            cubeShadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            cubeShadowWrite.descriptorCount = 1;
            cubeShadowWrite.pImageInfo = &cubeShadowInfo;
            descriptorWrites.push_back(cubeShadowWrite);
        }

        // Probe GI irradiance atlas (binding = 5) - lives in GENERAL layout
        if (probeIrradianceView != VK_NULL_HANDLE && probeIrradianceSampler != VK_NULL_HANDLE) {
            VkDescriptorImageInfo probeIrrInfo{};
            probeIrrInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            probeIrrInfo.imageView = probeIrradianceView;
            probeIrrInfo.sampler = probeIrradianceSampler;

            VkWriteDescriptorSet probeIrrWrite{};
            probeIrrWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            probeIrrWrite.dstSet = descriptorSets[i];
            probeIrrWrite.dstBinding = 5;
            probeIrrWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            probeIrrWrite.descriptorCount = 1;
            probeIrrWrite.pImageInfo = &probeIrrInfo;
            descriptorWrites.push_back(probeIrrWrite);
        }

        // Probe GI depth atlas (binding = 6)
        if (probeDepthView != VK_NULL_HANDLE && probeDepthSampler != VK_NULL_HANDLE) {
            VkDescriptorImageInfo probeDepthInfo{};
            probeDepthInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            probeDepthInfo.imageView = probeDepthView;
            probeDepthInfo.sampler = probeDepthSampler;

            VkWriteDescriptorSet probeDepthWrite{};
            probeDepthWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            probeDepthWrite.dstSet = descriptorSets[i];
            probeDepthWrite.dstBinding = 6;
            probeDepthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            probeDepthWrite.descriptorCount = 1;
            probeDepthWrite.pImageInfo = &probeDepthInfo;
            descriptorWrites.push_back(probeDepthWrite);
        }

        // Probe volume params UBO (binding = 7)
        if (probeParamsBuffer != VK_NULL_HANDLE) {
            VkDescriptorBufferInfo probeParamsInfo{};
            probeParamsInfo.buffer = probeParamsBuffer;
            probeParamsInfo.offset = 0;
            probeParamsInfo.range = sizeof(VolumeProbe);

            VkWriteDescriptorSet probeParamsWrite{};
            probeParamsWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            probeParamsWrite.dstSet = descriptorSets[i];
            probeParamsWrite.dstBinding = 7;
            probeParamsWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            probeParamsWrite.descriptorCount = 1;
            probeParamsWrite.pBufferInfo = &probeParamsInfo;
            descriptorWrites.push_back(probeParamsWrite);
        }

        // Skybox cubemap (binding = 8) - glass environment reflections
        if (skyboxView != VK_NULL_HANDLE && skyboxSampler != VK_NULL_HANDLE) {
            VkDescriptorImageInfo skyboxInfo{};
            skyboxInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            skyboxInfo.imageView = skyboxView;
            skyboxInfo.sampler = skyboxSampler;

            VkWriteDescriptorSet skyboxWrite{};
            skyboxWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            skyboxWrite.dstSet = descriptorSets[i];
            skyboxWrite.dstBinding = 8;
            skyboxWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            skyboxWrite.descriptorCount = 1;
            skyboxWrite.pImageInfo = &skyboxInfo;
            descriptorWrites.push_back(skyboxWrite);
        }
		// Update all descriptors
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
