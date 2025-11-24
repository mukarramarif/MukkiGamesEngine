#pragma once
#include <vulkan/vulkan.h>
#include "../Core/VkDevice.h"
#include <vector>
#include <array>
#include <glm/glm.hpp>
class VkDescriptorBoss
{
public:
	struct uniformBufferObject {
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 proj;
	};
	VkDescriptorBoss(const Device* device, uint32_t maxSets);
	void createDescriptorPool(uint32_t maxSets);
	VkDescriptorPool getDescriptorPool() const { return descriptorPool; }
	void createDescriptorSets(VkDescriptorSetLayout descriptorSetLayout, uint32_t count, std::vector<VkDescriptorSet>& descriptorSets);
	void destroyDescriptorPool();
	~VkDescriptorBoss();
	void cleanup();
private:
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	const Device* device = nullptr;


};