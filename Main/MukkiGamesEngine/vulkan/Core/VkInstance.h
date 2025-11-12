#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

class Instance {
public:
	Instance();
	
	Instance(const VkInstance& instance);
	Instance& operator=(const VkInstance& instance);
	void createInstance();
	void init();
	void cleanup();

	VkInstance getInstance() const { return instance; }
	bool isValidEnabled() const { return validEnabled; }
	const std::vector<std::string>& getEnabledExtensions() { return enabledExtensions; }
	static Instance* getInstancePtr() { return instancePtr; }
	bool enableValiationLayers = true;
	bool checkValidationLayersSupport();
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	const std::vector<const char*> getValidationLayers();
private:
	VkInstance instance;
	~Instance();
	VkDebugMarkerMarkerInfoEXT debugMessenger;
	bool validEnabled;
	std::vector<std::string> enabledExtensions;
	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};
	std::vector<const char*> getRequiredExtensions();

	static Instance* instancePtr;
};