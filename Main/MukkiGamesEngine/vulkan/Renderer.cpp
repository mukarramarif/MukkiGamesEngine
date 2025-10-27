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
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice,getSurface());

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.vertexPipelineStoresAndAtomics = VK_TRUE;
	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();

	createInfo.pEnabledFeatures = &deviceFeatures;

	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device!");
	}

	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
	graphicsQueueFamilyIndex = indices.graphicsFamily.value();
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

VkSurfaceKHR VulkanRenderer::getSurface()
{
	return surface;
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
