#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include "VkDevice.h"

class VulkanSwap {
private:
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;
	Device* devicePtr;  // Store pointer to Device wrapper
	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;  // ADD THIS
	std::vector<VkFramebuffer> swapChainFramebuffers;  // CHANGE: Multiple framebuffers
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	GLFWwindow* window;

public:
	void initSwap(Device& device, const VkSurfaceKHR& vkSurface, GLFWwindow* window);
	void createImageViews();
	void createFramebuffers(VkRenderPass renderPass, VkImageView depthImageView = VK_NULL_HANDLE);
	void cleanup();
	void resizeSwapChain(Device& device, const VkSurfaceKHR& vkSurface, GLFWwindow* window);
	
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);
	
	// Getters
	VkFormat getSwapChainImageFormat() const { return swapChainImageFormat; }
	std::vector<VkImage>& getSwapChainImages() { return swapChainImages; }
	std::vector<VkImageView>& getSwapChainImageViews() { return swapChainImageViews; }
	std::vector<VkFramebuffer>& getSwapChainFramebuffers() { return swapChainFramebuffers; }
	VkSwapchainKHR getSwapChain() const { return swapChain; }
	VkExtent2D getSwapChainExtent() const { return swapChainExtent; }
	size_t getImageCount() const { return swapChainImages.size(); }
};