#pragma once
#include "Renderer.h"
#include <memory>
#include <vector>
#include <string>

class VulkanApplication;
class TextureManager;
class BufferManager;
struct Light;
class Camera;
struct Model;

class VulkanRenderer : public MUKKI::Renderer {
public:
    VulkanRenderer();
    ~VulkanRenderer() override;

    void init(const RenderConfig& config) override;
    void shutdown() override;
    void run() override;

    void beginFrame() override;
    void endFrame() override;

    void addObject(uint32_t id, const ::Model& model, const glm::mat4& transform) override;
    void removeObject(uint32_t id) override;

    void setLights(const std::vector<Light>& lights, float ambientStrength) override;
    void setCamera(const Camera& camera) override;
    void setSkybox(const std::string& skyBoxPath) override;

    void setRenderMode(Mode mode) override;
    void getRenderMode() const override;

    void* getNativeWindow() const override;
    void onResize(int width, int height) override;

    float getFrameTime() const override;
    uint32_t getFrameCount() const override;

    void* getNativeCommandBuffer() override;
    void* getNativeDevice() override;
    TextureManager* getTextureManager() override;
    BufferManager* getBufferManager() override;

private:
    std::unique_ptr<VulkanApplication> m_app;
    bool m_initialized = false;
    Mode m_currentMode = Mode::GRAPHICS;
};
