#include "Renderer.h"
#include "HelperFunction.h"
#include <stdexcept>
void VulkanRenderer::init(Window* window)
{
	createInstance();
	pickPhysicalDevice();
	createLogicalDevice();
	createCommandPool();
	createCommandBuffers();
	createRenderPass();
	createGraphicsPipeline();
	createTexture("texture.jpg");
	window->init(800, 600, "Vulkan Window");


}

void VulkanRenderer::createInstance()
{
	// Implementation for creating Vulkan instance
	if (enableValiationLayers && !checkValidationLayers()) {
		throw std::runtime_error("Validation layers requested, but not available!");
	}
	VkApplicationInfo appInfo{};
	
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else {
		createInfo.enabledLayerCount = 0;

		createInfo.pNext = nullptr;
	}

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}
}
void VulkanRenderer::pickPhysicalDevice()
{
	// Implementation for selecting physical device
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
	for (const auto& device : devices) {
		if (isDeviceSuitable(device)) {
			physicalDevice = device;
			break;
		}
	}
	if (physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("failed to find a suitable GPU!");
	}
}
void VulkanRenderer::createLogicalDevice()
{
	// Implementation for creating logical device
}
void VulkanRenderer::createCommandPool()
{
	// Implementation for creating command pool
}
void VulkanRenderer::createCommandBuffers()
{
	// Implementation for creating command buffers
}
void VulkanRenderer::createRenderPass()
{
	// Implementation for creating render pass
}
	
VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
{
	// Implementation for creating shader module
	return VkShaderModule{};
}
void VulkanRenderer::createGraphicsPipeline()
{
	// Implementation for creating graphics pipeline
}
void VulkanRenderer::createTexture(const char* filename)
{
	// Implementation for creating texture
}
void VulkanRenderer::createTextureImage()
{
	// Implementation for creating texture image
}
void VulkanRenderer::createTextureImageView()
{
	// Implementation for creating texture image view
}
void VulkanRenderer::createTextureSampler()
{
	// Implementation for creating texture sampler
}
//     // Rendering loop
void VulkanRenderer::drawFrame()
{
	// Implementation for drawing a frame
}
//     // Cleanup
void VulkanRenderer::cleanup()
{
	// Implementation for cleaning up Vulkan resources
}
void VulkanRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	// Implementation for recording command buffer
}
