#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <vulkan/vulkan.h>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#define CLAMP(Val, Start, END) std::min(std::max(Val, Start), END)
namespace EngineUtils {
    inline std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file: " + filename);
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }
    inline int GetBytesPerTexComponent(VkFormat format) {
		switch (format)
		{
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R8_UNORM:
			return 1;
		case VK_FORMAT_R16_SFLOAT:
			return 2;
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R16G16_SNORM:
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SNORM:
		case VK_FORMAT_R8G8B8A8_SRGB:
			return 4;
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			return 4 * sizeof(uint16_t);
		case VK_FORMAT_R32G32B32_SFLOAT:
			return 3 * sizeof(float);
		case VK_FORMAT_R8G8B8_SRGB:
			return 3;
		case VK_FORMAT_R32G32B32A32_SFLOAT:
			return 4 * sizeof(float);
		default:
			std::cout << "Unsupported format in GetBytesPerTexComponent " << format << " \n";
			exit(1);
		}

		return 0;
    }

}