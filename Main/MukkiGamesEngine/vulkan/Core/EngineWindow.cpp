#include "EngineWindow.h"

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <sstream>
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

    if (!glfwInit()) {
        throw std::runtime_error("GLFW initialization failed");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

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
    if (!window) {
        throw std::runtime_error("createSurface called but GLFW window is null");
    }

    VkResult res = glfwCreateWindowSurface(instance, window, nullptr, &surface);
    if (res != VK_SUCCESS) {
        std::ostringstream os;
        os << "glfwCreateWindowSurface failed (code=" << res << ")";
        throw std::runtime_error(os.str());
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