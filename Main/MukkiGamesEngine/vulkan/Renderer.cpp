#include "Renderer.h"

#include <stdexcept>
#include "Core/VkInstance.h"
#include <array>
void VulkanRenderer::init(Window* window)
{
	/*createInstance();
	pickPhysicalDevice();
	createLogicalDevice();
	createCommandPool();
	createCommandBuffers();
	createRenderPass();
	createGraphicsPipeline();
	createTexture("texture.jpg");*/
	instance.createInstance();

	swapChain.initSwap(device, getSurface(), window->getGLFWwindow());

	window->init(800, 600, "Vulkan Window");



}

//void VulkanRenderer::createInstance()
//{
//	// Implementation for creating Vulkan instance
//	if (enableValiationLayers && !checkValidationLayers()) {
//		throw std::runtime_error("Validation layers requested, but not available!");
//	}
//	VkApplicationInfo appInfo{};
//	
//	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
//	appInfo.pApplicationName = "Hello Triangle";
//	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
//	appInfo.pEngineName = "No Engine";
//	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
//	appInfo.apiVersion = VK_API_VERSION_1_0;
//
//	VkInstanceCreateInfo createInfo{};
//	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
//	createInfo.pApplicationInfo = &appInfo;
//
//	auto extensions = getRequiredExtensions();
//	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
//	createInfo.ppEnabledExtensionNames = extensions.data();
//
//	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
//	if (enableValidationLayers) {
//		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
//		createInfo.ppEnabledLayerNames = validationLayers.data();
//
//		populateDebugMessengerCreateInfo(debugCreateInfo);
//		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
//	}
//	else {
//		createInfo.enabledLayerCount = 0;
//
//		createInfo.pNext = nullptr;
//	}
//
//	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
//		throw std::runtime_error("failed to create instance!");
//	}
//}
//void VulkanRenderer::pickPhysicalDevice()
//{
//	// Implementation for selecting physical device
//	uint32_t deviceCount = 0;
//	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
//	if (deviceCount == 0) {
//		throw std::runtime_error("failed to find GPUs with Vulkan support!");
//	}
//	std::vector<VkPhysicalDevice> devices(deviceCount);
//	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
//	for (const auto& device : devices) {
//		if (isDeviceSuitable(device)) {
//			physicalDevice = device;
//			break;
//		}
//	}
//	if (physicalDevice == VK_NULL_HANDLE) {
//		throw std::runtime_error("failed to find a suitable GPU!");
//	}
//}
//void VulkanRenderer::createLogicalDevice()
//
//{
//	QueueFamilyIndices indices = findQueueFamilies(physicalDevice,getSurface());
//
//	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
//	uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };
//
//	float queuePriority = 1.0f;
//	for (uint32_t queueFamily : uniqueQueueFamilies) {
//		VkDeviceQueueCreateInfo queueCreateInfo{};
//		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
//		queueCreateInfo.queueFamilyIndex = queueFamily;
//		queueCreateInfo.queueCount = 1;
//		queueCreateInfo.pQueuePriorities = &queuePriority;
//		queueCreateInfos.push_back(queueCreateInfo);
//	}
//
//	VkPhysicalDeviceFeatures deviceFeatures{};
//	deviceFeatures.samplerAnisotropy = VK_TRUE;
//	deviceFeatures.vertexPipelineStoresAndAtomics = VK_TRUE;
//	VkDeviceCreateInfo createInfo{};
//	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
//
//	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
//	createInfo.pQueueCreateInfos = queueCreateInfos.data();
//
//	createInfo.pEnabledFeatures = &deviceFeatures;
//
//	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
//	createInfo.ppEnabledExtensionNames = deviceExtensions.data();
//
//	if (enableValidationLayers) {
//		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
//		createInfo.ppEnabledLayerNames = validationLayers.data();
//	}
//	else {
//		createInfo.enabledLayerCount = 0;
//	}
//
//	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
//		throw std::runtime_error("failed to create logical device!");
//	}
//
//	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
//	vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
//	graphicsQueueFamilyIndex = indices.graphicsFamily.value();
//	// Implementation for creating logical device
//}
void VulkanRenderer::createCommandPool()
{
	// Implementation for creating command pool
	QueueFamilyIndices queueFamilyIndices = device.findQueueFamilies(physicalDevice);
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
	if (vkCreateCommandPool(device.getDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool!");
	}
}
void VulkanRenderer::createCommandBuffers()
{
	// Implementation for creating command buffers
	commandBuffers.resize(swapChain.getSwapChainImages().size());
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();
	if (vkAllocateCommandBuffers(device.getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}
}
VkFormat VulkanRenderer::findDepthFormat()
{
	return VkFormat();
}
VkFormat VulkanRenderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	return VkFormat();
}
void VulkanRenderer::createRenderPass()
{
	// Implementation for creating render pass
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapChain.getSwapChainImageFormat();
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = findDepthFormat();
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device.getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}
}

void VulkanRenderer::createDescriptorSetLayout() {
	// UBO binding (binding = 0)
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// Sampler binding (binding = 1)
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;  // Changed to fragment shader

	// SSBO binding (binding = 2)
	VkDescriptorSetLayoutBinding ssboLayoutBinding{};
	ssboLayoutBinding.binding = 2;  // Changed to binding 2
	ssboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	ssboLayoutBinding.descriptorCount = 1;
	ssboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
		uboLayoutBinding,
		samplerLayoutBinding,
		ssboLayoutBinding
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(device.getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	if (instance.enableValiationLayers) {
		// Logging the descriptor set layout details
		std::cout << "Descriptor Set Layout created successfully!" << std::endl;
		for (const auto& binding : bindings) {
			std::cout << "Binding: " << binding.binding
				<< ", Type: " << binding.descriptorType
				<< ", Count: " << binding.descriptorCount
				<< ", Stage Flags: " << binding.stageFlags << std::endl;
		}
		std::cout << "Descriptor Set Layout: " << descriptorSetLayout << std::endl;
	}
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
