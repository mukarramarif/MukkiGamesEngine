#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <glm/glm.hpp>
#include <iostream>
struct Vertex{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;
	glm::vec3 normal;
	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}
	static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);
		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);
		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(Vertex, normal);
		return attributeDescriptions;
	}
	static void printLayout() {
		std::cout << "\n=== Vertex Structure Layout ===" << std::endl;
		std::cout << "Total size: " << sizeof(Vertex) << " bytes" << std::endl;
		std::cout << "pos offset: " << offsetof(Vertex, pos) << " bytes" << std::endl;
		std::cout << "color offset: " << offsetof(Vertex, color) << " bytes" << std::endl;
		std::cout << "texCoord offset: " << offsetof(Vertex, texCoord) << " bytes" << std::endl;
		std::cout << "Expected layout:" << std::endl;
		std::cout << "  pos: 0-11 (12 bytes)" << std::endl;
		std::cout << "  color: 12-23 (12 bytes)" << std::endl;
		std::cout << "  texCoord: 24-31 (8 bytes)" << std::endl;
	}
	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && texCoord == other.texCoord;
	}
};
