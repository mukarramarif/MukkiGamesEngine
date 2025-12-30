#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

class EngineWindow {
private:
	struct renderData {
		GLFWwindow* window;
		int width;
		int height;
	};
	renderData renderData{};
	const char* title;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
public:
	~EngineWindow();

	void init(int w, int h, const char* t);
	GLFWwindow* getGLFWwindow();
	const int getWidth();
	const int getHeight();

	VkSurfaceKHR createSurface(VkInstance instance);
	VkSurfaceKHR getSurface() const { return surface; }
	void cleanup();

    bool framebufferResized = false;
};