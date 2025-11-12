#pragma once
#include <vulkan/vulkan.h>
#include "Device.h"
#include <string>
class VulkanPipeline {
	VulkanPipeline(VulkanDevice* device, VkRenderPass renderPass, const std::string& vertShaderPath, const std::string& fragShaderPath);
	~VulkanPipeline();

};