#include "EngineWindow.h"

Window::~Window()
{
	glfwDestroyWindow(window);
	glfwTerminate();

}
void Window::init(int w, int h, const char* t)
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
GLFWwindow* Window::getGLFWwindow()
{
	return window;
}
const int Window::getWidth()
{
	return width;
}	
const int Window::getHeight()
{
	return height;
}
static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	app->framebufferResized = true;
}