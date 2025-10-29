#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "vulkan/HelperFunction.h"
class Instance {
public:
	Instance();
	~Instance();
	Instance(const VkInstance& instance);
	Instance& operator=(const VkInstance& instance);
	
	void init();
	void cleanup();

	VkInstance getInstance() const { return instance; }
	bool isValidEnabled() const { return validEnabled; }
	const std::vector<std::string>& getEnabledExtensions() { return enabledExtensions; }
private:
	VkInstance instance;
	VkDebugMarkerMarkerInfoEXT debugMessenger;
	bool validEnabled;
	std::vector<std::string> enabledExtensions;
	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};
	void createInstance();
	bool checkValidationLayersSupport();
	std::vector<const char*> getRequiredExtensions();
	
};