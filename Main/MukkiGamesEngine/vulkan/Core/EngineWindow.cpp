#include "EngineWindow.h"

#include <vulkan/vulkan.h>
#include <stdexcept>

static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<EngineWindow*>(glfwGetWindowUserPointer(window));
	app->framebufferResized = true;
}

EngineWindow::~EngineWindow()
{
	cleanup();
}
void EngineWindow::init(int w, int h, const char* t)
{
	width = w;
	height = h;
	title = t;

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width, height, title, nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}
GLFWwindow* EngineWindow::getGLFWwindow()
{
	return window;
}
const int EngineWindow::getWidth()
{
	return width;
}	
const int EngineWindow::getHeight()
{
	return height;
}

VkSurfaceKHR EngineWindow::createSurface(VkInstance instance)
{
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
    return surface;
}

void EngineWindow::cleanup()
{
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
}