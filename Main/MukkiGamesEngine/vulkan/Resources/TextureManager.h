#pragma once
#include "vulkan/vulkan.h"
#include "../Core/VkDevice.h"
#include <stb_image.h>
#include <string>
class CommandBufferManager;
class BufferManager;
class TextureManager {
public:
	TextureManager();
	~TextureManager();
	void init(const Device& device,CommandBufferManager& commandBuffer, BufferManager& buffermanager);
	void cleanup();
	void createImage(uint32_t width, uint32_t height, VkFormat format,
		VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
		VkImage& image, VkDeviceMemory& imageMemory);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	void transitionImageLayout(VkImage image, VkFormat format,
		VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void createTextureImage(const std::string& filePath, VkImage& textureImage, VkDeviceMemory& textureImageMemory);
	void createTextureSampler(VkSampler& sampler);
	void createDebugTextureImage(VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkImageView& imageView);
	void createdepthResources(VkImage& depthImage, VkDeviceMemory& depthImageMemory, VkImageView& depthImageView, uint32_t width, uint32_t height);
	void destroyImage(VkImage image, VkDeviceMemory imageMemory);
	void destroySampler(VkSampler sampler);
	void destroyImageView(VkImageView imageView);
private:
	const Device* device = nullptr;
	CommandBufferManager* commandBufferManager;
	BufferManager* bufferManager;
	VkFormat findDepthFormat();
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	bool hasStencilComponent(VkFormat format);

};