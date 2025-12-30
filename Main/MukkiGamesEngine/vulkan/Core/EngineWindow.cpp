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
    renderData.width = w;
    renderData.height = h;
    title = t;

    if (!glfwInit()) {
        throw std::runtime_error("GLFW initialization failed");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    renderData.window = glfwCreateWindow(renderData.width, renderData.height, title, nullptr, nullptr);
    if (!renderData.window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(renderData.window, this);
    glfwSetFramebufferSizeCallback(renderData.window, framebufferResizeCallback);
}

GLFWwindow* EngineWindow::getGLFWwindow()
{
    return renderData.window;
}

const int EngineWindow::getWidth()
{
    return renderData.width;
}

const int EngineWindow::getHeight()
{
    return renderData.height;
}

VkSurfaceKHR EngineWindow::createSurface(VkInstance instance)
{
    if (!renderData.window) {
        throw std::runtime_error("createSurface called but GLFW window is null");
    }

    VkResult res = glfwCreateWindowSurface(instance, renderData.window, nullptr, &surface);
    if (res != VK_SUCCESS) {
        std::ostringstream os;
        os << "glfwCreateWindowSurface failed (code=" << res << ")";
        throw std::runtime_error(os.str());
    }
    return surface;
}

void EngineWindow::cleanup()
{
    if (renderData.window) {
        glfwDestroyWindow(renderData.window);
        renderData.window = nullptr;
    }
    glfwTerminate();
}