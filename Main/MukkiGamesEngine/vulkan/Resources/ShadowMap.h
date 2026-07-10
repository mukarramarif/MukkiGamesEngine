#pragma once
#include <vulkan/vulkan.h>
#include "../Core/VkDevice.h"
#include <vector>
#include <array>
#include <glm/glm.hpp>

class ShadowMap {
public:
	ShadowMap() = default;
	~ShadowMap();

	void init(Device* device, uint32_t shadowMapSize = 2048);
	void cleanup();

	VkImageView getShadowMapImageView() const { return shadowMapImageView; }
	VkSampler getShadowSampler() const { return shadowSampler; }
	VkRenderPass getRenderPass() const { return shadowRenderPass; }
	VkPipelineLayout getPipelineLayout() const { return shadowPipelineLayout; }
	VkPipeline getPipeline() const { return shadowPipeline; }
	VkFramebuffer getFramebuffer() const { return shadowFramebuffer; }

	uint32_t getShadowMapSize() const { return shadowMapSize; }
	VkExtent2D getExtent() const { return { shadowMapSize, shadowMapSize }; }
	VkImage getShadowImage() const { return shadowMapImage; }
	VkImage getDepthImage() const { return shadowMapDepthImage; }

private:
	Device* device = nullptr;
	uint32_t shadowMapSize = 2048;

	VkImage shadowMapImage = VK_NULL_HANDLE;           // R32_SFLOAT (color, for sampling)
	VkDeviceMemory shadowMapImageMemory = VK_NULL_HANDLE;
	VkImageView shadowMapImageView = VK_NULL_HANDLE;   // R32_SFLOAT color view
	VkImage shadowMapDepthImage = VK_NULL_HANDLE;      // D32_SFLOAT (temporary depth buffer for the shadow pass)
	VkDeviceMemory shadowMapDepthMemory = VK_NULL_HANDLE;
	VkImageView shadowMapDepthImageView = VK_NULL_HANDLE;
	VkSampler shadowSampler = VK_NULL_HANDLE;           // regular (non-comparison)
	VkRenderPass shadowRenderPass = VK_NULL_HANDLE;
	VkPipelineLayout shadowPipelineLayout = VK_NULL_HANDLE;
	VkPipeline shadowPipeline = VK_NULL_HANDLE;
	VkFramebuffer shadowFramebuffer = VK_NULL_HANDLE;


	VkShaderModule vertShaderModule = VK_NULL_HANDLE;
	VkShaderModule fragShaderModule = VK_NULL_HANDLE;

	VkFormat findDepthFormat();
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	void createShadowImage();
	void createDepthImage();
	void createShadowRenderPass();
	void createShadowSampler();
	void createShadowFramebuffer();
	void createShaderModules();
	void createPipelineLayout();
	void createPipeline();
};
