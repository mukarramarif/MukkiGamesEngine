#include "VulkanRenderer.h"
#include "../vulkan/Core/VkApplication.h"
#include "../vulkan/Resources/TextureManager.h"
#include "../vulkan/Resources/BufferManager.h"
#include "../vulkan/Resources/Camera.h"
#include "../vulkan/objects/lights.h"

VulkanRenderer::VulkanRenderer() = default;
VulkanRenderer::~VulkanRenderer() { shutdown(); }

void VulkanRenderer::init(const RenderConfig& config)
{
    if (m_initialized) return;
    m_app = std::make_unique<VulkanApplication>();
    m_app->init(config);
    m_initialized = true;
}

void VulkanRenderer::shutdown()
{
    if (!m_initialized) return;
    m_app->shutdown();
    m_app.reset();
    m_initialized = false;
}

void VulkanRenderer::run()
{
    if (!m_initialized) return;
    m_app->run();
}

void VulkanRenderer::beginFrame() {}
void VulkanRenderer::endFrame() {}

void VulkanRenderer::addObject(uint32_t id, const ::Model& model, const glm::mat4& transform)
{
    (void)id; (void)model; (void)transform;
}

void VulkanRenderer::removeObject(uint32_t id)
{
    (void)id;
}

void VulkanRenderer::setLights(const std::vector<Light>& lights, float ambientStrength)
{
    if (!m_app) return;
    m_app->setLights(lights, ambientStrength);
}

void VulkanRenderer::setCamera(const Camera& camera)
{
    if (!m_app) return;
    m_app->setCamera(camera);
}

void VulkanRenderer::setSkybox(const std::string& skyBoxPath)
{
    if (!m_app) return;
    m_app->setSkybox(skyBoxPath);
}

void VulkanRenderer::setRenderMode(Mode mode)
{
    m_currentMode = mode;
    if (!m_app) return;
    m_app->setRenderMode(static_cast<int>(mode));
}

void VulkanRenderer::getRenderMode() const
{
}

void* VulkanRenderer::getNativeWindow() const
{
    return m_app ? m_app->getNativeWindow() : nullptr;
}

void VulkanRenderer::onResize(int width, int height)
{
    if (!m_app) return;
    m_app->onResize(width, height);
}

float VulkanRenderer::getFrameTime() const
{
    return m_app ? m_app->getFrameTime() : 0.0f;
}

uint32_t VulkanRenderer::getFrameCount() const
{
    return m_app ? m_app->getFrameCount() : 0;
}

void* VulkanRenderer::getNativeCommandBuffer()
{
    return m_app ? m_app->getCurrentCommandBuffer() : nullptr;
}

void* VulkanRenderer::getNativeDevice()
{
    return m_app ? m_app->getNativeDevice() : nullptr;
}

TextureManager* VulkanRenderer::getTextureManager()
{
    return m_app ? m_app->getTextureManagerPtr() : nullptr;
}

BufferManager* VulkanRenderer::getBufferManager()
{
    return m_app ? m_app->getBufferManagerPtr() : nullptr;
}
