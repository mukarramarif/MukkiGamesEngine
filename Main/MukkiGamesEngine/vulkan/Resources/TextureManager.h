#pragma once
#include "vulkan/vulkan.h"
#include "../Core/VkDevice.h"
#include <stb_image.h>
#include <string>
#include <array>

class CommandBufferManager;
class BufferManager;

enum class CubemapLayout {
	HorizontalCross,  // 4x3 layout (standard cross)
	VerticalCross,    // 3x4 layout
	HorizontalStrip,  // 6x1 layout (all faces in a row)
	VerticalStrip     // 1x6 layout (all faces in a column)
};

class TextureManager {
public:
	TextureManager();
	~TextureManager();
	void init(const Device& device, CommandBufferManager& commandBuffer, BufferManager& buffermanager);
	void cleanup();
	void createImage(uint32_t width, uint32_t height, VkFormat format,
		VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
		VkImage& image, VkDeviceMemory& imageMemory, bool isCubemap = false);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, bool isCubemap = false);
	void transitionImageLayout(VkImage image, VkFormat format,
		VkImageLayout oldLayout, VkImageLayout newLayout, bool isCubemap = false);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, bool isCubemap = false);
	void createTextureImage(const std::string& filePath, VkImage& textureImage, VkDeviceMemory& textureImageMemory);
	void createCubemapImage(const std::string& filePath, VkImage& cubemapImage, VkDeviceMemory& cubemapImageMemory,
		CubemapLayout layout = CubemapLayout::HorizontalCross);
	void createTextureSampler(VkSampler& sampler, bool isCubemap = false);
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

	void extractCubemapFace(stbi_uc* srcPixels, int srcWidth, int srcHeight,
		stbi_uc* dstPixels, int faceSize, int faceIndex, CubemapLayout layout);
};