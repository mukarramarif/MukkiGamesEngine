#include "VkInstance.h"
#include <iostream>
#include <GLFW/glfw3.h>
Instance* Instance::instancePtr = nullptr;
Instance::Instance() : instance(VK_NULL_HANDLE), validEnabled(false) {

}
Instance::~Instance() {
	cleanup();
}
void Instance::cleanup() {
	if (instance != VK_NULL_HANDLE) {
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}
}
Instance::Instance(const VkInstance& instance) : instance(instance) {
	validEnabled = true;

}
void Instance::createInstance() {
	if(enableValidationLayers && !checkValidationLayersSupport()) {
		throw std::runtime_error("validation layers requested, but not available!");
	}
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Mukki Games Engine";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	auto extensions = getRequiredExtensions();
	const bool enablePortability = (std::getenv("ENABLE_PORTABILITY_ENUM") != nullptr);
	if (enablePortability) {
		extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
	}

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	//createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();
	if (enablePortability) {
		createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	}
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
	

	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
	if (result != VK_SUCCESS) {
		std::cerr << "Unable to create VK Instance: " << result << std::endl;
		throw std::runtime_error("failed to create an instance!");
	}
	else {
		std::cout << "Vk instance created successfully - " << result << std::endl;
	}
}
bool Instance::checkValidationLayersSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
};
void Instance::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)

{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
}

const std::vector<const char*> Instance::getValidationLayers()
{
	return validationLayers;
}

std::vector<const char*> Instance::getRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}
