#pragma once
#include <vulkan/vulkan.h>
#include "Core/VkDevice.h"
#include "Core/SwapChain.h"
#include "Core/VkInstance.h"
#include "Core/EngineWindow.h"
#include <string>
class Device;

class VulkanPipeline {
public:
	VulkanPipeline(Device* device, VkRenderPass renderPass, const std::string& vertShaderPath, const std::string& fragShaderPath);
	~VulkanPipeline();
	void recreate(const VulkanSwap& swapChain);
	VkRenderPass getRenderPass() const { return renderPass; }
	VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
	VkPipeline getGraphicsPipeline() const { return graphicsPipeline; }
	VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }
private:
	Device* device;
	VulkanSwap* vkSwap;
	Window* Window;
	Instance* instance;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipeline graphicsPipeline;
	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;
	void createDescriptorSetLayout();

	void createRenderPass(VkFormat swapChainImageFormat);
	void createGraphicsPipeline(VkExtent2D swapChainExtent, const std::string& vertShaderPath, const std::string& fragShaderPath);
	VkShaderModule createShaderModule(const std::vector<char>& code);
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat findDepthFormat();
};