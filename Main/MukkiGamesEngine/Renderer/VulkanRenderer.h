#pragma once
#include "Renderer.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

class EngineWindow;
class Device;
class VulkanSwap;
class VulkanRenderPass;
class VulkanPipeline;
class CommandBufferManager;
class BufferManager;
class TextureManager;
class ObjectLoader;
class VkDescriptorBoss;
class SkyBox;
class ShadowMap;
class ComputePipeline;
class RayTracingPipeline;
class RayTracingAS;
class DeletionQueue;
struct Model;
struct Light;
struct Camera;

class VulkanRenderer : public MUKKI::Renderer {
public:
    VulkanRenderer();
    ~VulkanRenderer() override;

    void init(const RenderConfig& config) override;
    void shutdown() override;
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
    void* getNativeCommandBuffer() override;
    void* getNativeDevice() override;
    class TextureManager* getTextureManager() override;
    class BufferManager* getBufferManager() override;
    void onResize(int width, int height) override;
    float getFrameTime() const override;
    uint32_t getFrameCount() const override;

private:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    struct ObjectGPUData {
        Model* model = nullptr;
        std::vector<VkBuffer> uniformBuffers;
        std::vector<VkDeviceMemory> uniformBuffersMemory;
        std::vector<void*> uniformBuffersMapped;
        std::vector<std::vector<VkBuffer>> materialUniformBuffers;
        std::vector<std::vector<VkDeviceMemory>> materialUniformBuffersMemory;
        std::vector<std::vector<void*>> materialUniformBuffersMapped;
        std::vector<std::vector<VkDescriptorSet>> descriptorSets;
    };

    // Vulkan-specific accessors for VkApplication (downcast to use)
    class Device* getDevicePtr();
    class VulkanSwap* getSwapChainPtr();
    class VulkanRenderPass* getRenderPassPtr();
    class CommandBufferManager* getCmdBufManagerPtr();
    VkRenderPass getRenderPass();
    class ShadowMap* getShadowMapPtr();

    bool initialized = false;

    // Owned Vulkan objects (nullptr until init)
    std::unique_ptr<EngineWindow> window;
    std::unique_ptr<Device> device;
    std::unique_ptr<VulkanSwap> swapChain;
    std::unique_ptr<VulkanRenderPass> renderPassObj;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::unique_ptr<VulkanPipeline> graphicsPipeline;
    std::unique_ptr<VulkanPipeline> additivePipeline;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    std::unique_ptr<CommandBufferManager> cmdBufManager;
    std::unique_ptr<BufferManager> bufferManager;
    std::unique_ptr<TextureManager> textureManager;
    std::unique_ptr<ObjectLoader> objectLoader;
    std::unique_ptr<VkDescriptorBoss> descriptorBoss;
    std::unique_ptr<SkyBox> skybox;
    std::unique_ptr<ShadowMap> shadowMap;
    DeletionQueue* deletionQueue = nullptr;

    std::unordered_map<uint32_t, ObjectGPUData> objects;
    std::vector<VkDescriptorSet> defaultDescriptorSets;

    // Shared UBOs
    std::vector<VkBuffer> sharedUniformBuffers;
    std::vector<VkDeviceMemory> sharedUniformBuffersMemory;
    std::vector<void*> sharedUniformBuffersMapped;
    std::vector<VkBuffer> defaultMaterialUniformBuffers;
    std::vector<VkDeviceMemory> defaultMaterialUniformBuffersMemory;
    std::vector<void*> defaultMaterialUniformBuffersMapped;

    // Sync
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    std::vector<VkImageLayout> swapChainImageLayouts;
    uint32_t currentFrame = 0;
    VkCommandBuffer currentCmdBuffer = VK_NULL_HANDLE;

    // State
    float mAmbientStrength = 0.1f;
    std::vector<Light> mLights;
    glm::mat4 mViewMatrix = glm::mat4(1.0f);
    glm::mat4 mProjMatrix = glm::mat4(1.0f);
    glm::vec3 mCameraPos = glm::vec3(0.0f);
    Mode mRenderMode = Mode::GRAPHICS;
    float mLastFrameTime = 0.0f;
    uint32_t mFrameCount = 0;
};
