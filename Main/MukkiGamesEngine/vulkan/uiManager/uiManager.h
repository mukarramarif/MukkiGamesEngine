#pragma once
#include <vulkan/vulkan.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>
#include "../Core/VkDevice.h"
#include "../Core/EngineWindow.h"

struct UIRenderData {
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	uint32_t queueFamily;
	VkQueue queue;
	VkRenderPass renderPass;
	VkCommandPool commandPool;
	VkDescriptorPool descriptorPool;
	uint32_t imageCount;
	uint32_t minImageCount;
};
struct ModelTransform {
	glm::vec3 rotation = glm::vec3(1.0f);
	glm::vec3 position = glm::vec3(1.0f);
	float scale = 100.0f;
	bool autoRotate = false;
	float autoRotateSpeed = 0.5f;
	int autoRotateAxis = 1;
};
class UIManager {
public:
	UIManager();
	~UIManager();
	
	void init(const UIRenderData& renderData, EngineWindow* window);
	void newFrame();
	void render(VkCommandBuffer commandBuffer);
	void cleanup();
	
	
	void renderDebugWindow(float fps, float deltaTime);
	void renderCameraInfo(const glm::vec3& position, const glm::vec3& front);
	void renderModelTransformWindow(ModelTransform& transform, float deltaTime);
private:
	VkDevice device = VK_NULL_HANDLE;
	VkDescriptorPool imguiPool = VK_NULL_HANDLE;
	bool initialized = false;
	
	void createDescriptorPool(const UIRenderData& renderData);
	void checkVkResult(VkResult err);
};