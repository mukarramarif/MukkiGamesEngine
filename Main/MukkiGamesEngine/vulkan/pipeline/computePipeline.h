#pragma once
#include <vulkan/vulkan.h>
#include "../Core/VkDevice.h"
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>


class Device;
struct ComputePushConstants {
	float iResolution[2];
	float iTime;
};
class ComputePipeline
{
public:
	ComputePipeline();
	~ComputePipeline();

	void createComputePipeline(Device* device, const std::string& computeShaderPath);
	void createDescriptorSetLayout(Device* device);
	void createDescriptorPool(Device* device, uint32_t maxSets);
	void createDescriptorSets(Device* device, VkImageView outputImageView);
	void cleanup(Device* device);

	VkPipeline getPipeline() const { return computePipeline; }
	VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
	VkDescriptorSet getDescriptorSet() const { return computeDescriptorSets; }

private:
	VkShaderModule createShaderModule(Device* device, const std::vector<char>& code);

	VkPipeline computePipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet computeDescriptorSets;
};


