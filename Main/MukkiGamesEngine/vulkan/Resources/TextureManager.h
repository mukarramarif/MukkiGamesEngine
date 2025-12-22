#pragma once
#include "vulkan/vulkan.h"
#include "../Core/VkDevice.h"
#include "../CommandBufferManager.h"
#include "BufferManager.h"

#include <string>

class TextureManager {
public:
	TextureManager();
	void init(const VkDevice& device,CommandBufferManager& commandBuffer, BufferManager& buffermanager);
private:
	~TextureManager();

};