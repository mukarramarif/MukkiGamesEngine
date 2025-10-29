#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "HelperFunction.h"
#include <vector>

class VulkanSwap {
private:
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;
	VkDevice device;
	std::vector<VkImage> swapChainImages;
public:
	void initSwap(const VkDevice& logicalDevice, const VkSurfaceKHR& vkSurface, GLFWwindow* window);
	void createSwapChain();
	void createImageViews();
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
};