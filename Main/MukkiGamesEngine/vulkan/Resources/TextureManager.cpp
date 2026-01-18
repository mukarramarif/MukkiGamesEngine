#include "TextureManager.h"
#include"../CommandBufferManager.h"
#include "BufferManager.h"
#include "BufferManager.h"
#include <stdexcept>

TextureManager::~TextureManager()
{
}
TextureManager::TextureManager()
{
}
void TextureManager::init(const Device& device, CommandBufferManager& commandBuffer, BufferManager& buffermanager)
{
	this->device= &device;
	this->commandBufferManager = &commandBuffer;
	this->bufferManager = &buffermanager;
}
void TextureManager::cleanup()
{
}
void TextureManager::createImage(uint32_t width, uint32_t height, VkFormat format,
	VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
	VkImage& image, VkDeviceMemory& imageMemory, bool isCubemap)
{
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = isCubemap ? 6 : 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.flags = isCubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

	if (vkCreateImage(device->getDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device->getDevice(), image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = device->findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(device->getDevice(), image, imageMemory, 0);

}
VkImageView TextureManager::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, bool isCubemap)
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = isCubemap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = isCubemap ? 6 : 1;
	VkImageView imageView;
	if (vkCreateImageView(device->getDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image view!");
	}
	return imageView;
}

void TextureManager::transitionImageLayout(VkImage image, VkFormat format,
	VkImageLayout oldLayout, VkImageLayout newLayout, bool isCubemap)
{
	VkCommandBuffer commandBuffer = commandBufferManager->beginSingleTimeCommands();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = isCubemap ? 6 : 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (hasStencilComponent(format)) {
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0,
		0, nullptr, 0, nullptr, 1, &barrier);

	commandBufferManager->endSingleTimeCommands(commandBuffer);
}

void TextureManager::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, bool isCubemap)
{
	VkCommandBuffer commandBuffer = commandBufferManager->beginSingleTimeCommands();

	if (isCubemap) {
		VkDeviceSize faceSize = width * height * 4;
		std::array<VkBufferImageCopy, 6> regions{};

		for (uint32_t i = 0; i < 6; i++) {
			regions[i].bufferOffset = i * faceSize;
			regions[i].bufferRowLength = 0;
			regions[i].bufferImageHeight = 0;
			regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			regions[i].imageSubresource.mipLevel = 0;
			regions[i].imageSubresource.baseArrayLayer = i;
			regions[i].imageSubresource.layerCount = 1;
			regions[i].imageOffset = { 0, 0, 0 };
			regions[i].imageExtent = { width, height, 1 };
		}

		vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(regions.size()), regions.data());
	}
	else {
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	commandBufferManager->endSingleTimeCommands(commandBuffer);
}

void TextureManager::createTextureImage(const std::string& filePath, VkImage& textureImage, VkDeviceMemory& textureImageMemory)
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	bufferManager->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(device->getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(device->getDevice(), stagingBufferMemory);

	stbi_image_free(pixels);

	createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(device->getDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device->getDevice(), stagingBufferMemory, nullptr);
}

void TextureManager::createTextureSampler(VkSampler& sampler, bool isCubemap)
{
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(device->getPhysicalDevice(), &properties);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = isCubemap ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = isCubemap ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = isCubemap ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	if (vkCreateSampler(device->getDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}

void TextureManager::createDebugTextureImage(VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkImageView& imageView)
{
	const uint32_t texWidth = 2;
	const uint32_t texHeight = 2;
	VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth * texHeight) * 4;
	unsigned char pixels[4] = { 4,60,255,255 };
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	bufferManager->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);
	void* data;
	vkMapMemory(device->getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(device->getDevice(), stagingBufferMemory);

	createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);
	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(stagingBuffer, textureImage, texWidth, texHeight);
	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	bufferManager->destroyBuffer(stagingBuffer, stagingBufferMemory);
	imageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}
void TextureManager::createdepthResources(VkImage& depthImage, VkDeviceMemory& depthImageMemory, VkImageView& depthImageView, uint32_t width, uint32_t height)
{
	VkFormat depthFormat = findDepthFormat();
	createImage(width,height,depthFormat, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depthImage, depthImageMemory);
	depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}
VkFormat TextureManager::findDepthFormat() {
	return findSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

VkFormat TextureManager::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(device->getPhysicalDevice(), format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}
void TextureManager::destroyImage(VkImage image, VkDeviceMemory imageMemory)
{
	if(image != VK_NULL_HANDLE) {
		vkDestroyImage(device->getDevice(), image, nullptr);
	}
	if(imageMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device->getDevice(), imageMemory, nullptr);
	}

}
void TextureManager::destroySampler(VkSampler sampler)
{
}
void TextureManager::destroyImageView(VkImageView imageView)
{
	if (imageView != VK_NULL_HANDLE) {
		vkDestroyImageView(device->getDevice(), imageView, nullptr);
		imageView = VK_NULL_HANDLE;
	}
}
bool TextureManager::hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void TextureManager::extractCubemapFace(stbi_uc* srcPixels, int srcWidth, int srcHeight,
	stbi_uc* dstPixels, int faceSize, int faceIndex, CubemapLayout layout)
{
	// Face order for Vulkan: +X (right), -X (left), +Y (top), -Y (bottom), +Z (front), -Z (back)
	int faceX = 0, faceY = 0;

	switch (layout) {
	case CubemapLayout::HorizontalCross:
		// Standard horizontal cross layout (4x3 grid):
		//        col0  col1  col2  col3
		// row0:  [ ]   [+Y]  [ ]   [ ]
		// row1:  [-X]  [+Z]  [+X]  [-Z]
		// row2:  [ ]   [-Y]  [ ]   [ ]
		switch (faceIndex) {
		case 0: faceX = 2; faceY = 1; break; // +X (right)
		case 1: faceX = 0; faceY = 1; break; // -X (left)
		case 2: faceX = 1; faceY = 0; break; // +Y (top)
		case 3: faceX = 1; faceY = 2; break; // -Y (bottom)
		case 4: faceX = 1; faceY = 1; break; // +Z (front)
		case 5: faceX = 3; faceY = 1; break; // -Z (back)
		}
		break;

	case CubemapLayout::VerticalCross:
		switch (faceIndex) {
		case 0: faceX = 2; faceY = 1; break;
		case 1: faceX = 0; faceY = 1; break;
		case 2: faceX = 1; faceY = 0; break;
		case 3: faceX = 1; faceY = 2; break;
		case 4: faceX = 1; faceY = 1; break;
		case 5: faceX = 1; faceY = 3; break;
		}
		break;

	case CubemapLayout::HorizontalStrip:
		faceX = faceIndex;
		faceY = 0;
		break;

	case CubemapLayout::VerticalStrip:
		faceX = 0;
		faceY = faceIndex;
		break;
	}

	int srcOffsetX = faceX * faceSize;
	int srcOffsetY = faceY * faceSize;

	// Bounds check - if reading outside image, fill with debug color
	for (int y = 0; y < faceSize; y++) {
		for (int x = 0; x < faceSize; x++) {
			int srcX = srcOffsetX + x;
			int srcY = srcOffsetY + y;
			int dstIdx = (y * faceSize + x) * 4;

			// Check if source coordinates are within bounds
			if (srcX >= srcWidth || srcY >= srcHeight || srcX < 0 || srcY < 0) {
				// Out of bounds - fill with magenta for debugging
				dstPixels[dstIdx + 0] = 255; // R
				dstPixels[dstIdx + 1] = 0;   // G
				dstPixels[dstIdx + 2] = 255; // B
				dstPixels[dstIdx + 3] = 255; // A
			}
			else {
				int srcIdx = (srcY * srcWidth + srcX) * 4;
				dstPixels[dstIdx + 0] = srcPixels[srcIdx + 0];
				dstPixels[dstIdx + 1] = srcPixels[srcIdx + 1];
				dstPixels[dstIdx + 2] = srcPixels[srcIdx + 2];
				dstPixels[dstIdx + 3] = srcPixels[srcIdx + 3];
			}
		}
	}
}

void TextureManager::createCubemapImage(const std::string& filePath,
	VkImage& cubemapImage, VkDeviceMemory& cubemapImageMemory, CubemapLayout layout)
{
	int texWidth = 0, texHeight = 0, texChannels = 0;
	stbi_uc* pixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		throw std::runtime_error("failed to load cubemap texture: " + filePath);
	}

	// Determine face size based on layout
	int faceSize = 0;
	switch (layout) {
	case CubemapLayout::HorizontalCross:
		faceSize = texWidth / 4;  // 4 columns
		break;
	case CubemapLayout::VerticalCross:
		faceSize = std::min(texWidth / 3, texHeight / 4);  // 3 columns
		break;
	case CubemapLayout::HorizontalStrip:
		faceSize = texWidth / 6;  // 6 columns
		break;
	case CubemapLayout::VerticalStrip:
		faceSize = texHeight / 6; // 6 rows
		break;
	}

	VkDeviceSize faceSizeBytes = static_cast<VkDeviceSize>(faceSize) * faceSize * 4;
	VkDeviceSize totalSize = faceSizeBytes * 6;

	// Allocate buffer for all 6 extracted faces
	std::vector<stbi_uc> faceData(totalSize);

	// Extract each face from the source image
	for (int i = 0; i < 6; i++) {
		extractCubemapFace(pixels, texWidth, texHeight,
			faceData.data() + (i * faceSizeBytes), faceSize, i, layout);
	}

	stbi_image_free(pixels);

	// Create staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	bufferManager->createBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(device->getDevice(), stagingBufferMemory, 0, totalSize, 0, &data);
	memcpy(data, faceData.data(), totalSize);
	vkUnmapMemory(device->getDevice(), stagingBufferMemory);

	// Create cubemap image
	createImage(faceSize, faceSize, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, cubemapImage, cubemapImageMemory, true);

	transitionImageLayout(cubemapImage, VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true);
	copyBufferToImage(stagingBuffer, cubemapImage, faceSize, faceSize, true);
	transitionImageLayout(cubemapImage, VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);

	vkDestroyBuffer(device->getDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device->getDevice(), stagingBufferMemory, nullptr);
}