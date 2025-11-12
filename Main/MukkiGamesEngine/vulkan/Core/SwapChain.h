#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include "VkDevice.h"
class VulkanSwap {
private:
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;
	VkDevice device;
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
public:
	void initSwap(Device& device, const VkSurfaceKHR& vkSurface, GLFWwindow* window);
	void createImageViews();
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);
	VkFormat getSwapChainImageFormat() const { return swapChainImageFormat; }
};