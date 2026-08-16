#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <glm/glm.hpp>
#include "../Core/VkDevice.h"

class ShadowCubeMap {
public:
    ShadowCubeMap();
    ~ShadowCubeMap();

    bool init(Device* device, uint32_t size = 1024, float farPlane = 100.0f);
    void cleanup();

    // Begin render pass for one cube face and bind the shadow pipeline
    void BindForWriting(VkCommandBuffer commandBuffer, uint32_t faceIndex);

    // Main-pass accessors (descriptor binding is done via VkDescriptorBoss)
    VkImageView getCubeMapImageView() const { return cubeMapImageView; }
    VkSampler  getCubeMapSampler()  const { return cubeMapSampler; }
    VkImage    getCubeMapImage()    const { return cubeMapImage; }
    uint32_t   getSize()            const { return cubeMapSize; }
    float      getFarPlane()        const { return farPlane; }
    VkPipelineLayout getPipelineLayout() const { return cubeMapPipelineLayout; }
    // Per-face view-projection matrix for the shadow pass
    static glm::mat4 computeFaceViewProj(uint32_t faceIndex,
                                         const glm::vec3& lightPos,
                                         float farPlane);

    static const glm::vec3 faceDirections[6];
    static const glm::vec3 faceUps[6];

private:
    Device* device = nullptr;
    uint32_t cubeMapSize = 1024;
    float    farPlane = 100.0f;

    VkImage              cubeMapImage = VK_NULL_HANDLE;
    VkDeviceMemory       cubeMapImageMemory = VK_NULL_HANDLE;
    VkImageView          cubeMapImageView = VK_NULL_HANDLE;              // CUBE view (sampling)
    std::array<VkImageView, 6> faceImageViews{};                         // 2D views (rendering)
    VkSampler            cubeMapSampler = VK_NULL_HANDLE;
    VkRenderPass         cubeMapRenderPass = VK_NULL_HANDLE;             // depth-only
    std::array<VkFramebuffer, 6> faceFramebuffers{};                     // one per face
    VkPipelineLayout     cubeMapPipelineLayout = VK_NULL_HANDLE;
    VkPipeline           cubeMapPipeline = VK_NULL_HANDLE;
    VkShaderModule       vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule       fragShaderModule = VK_NULL_HANDLE;

    VkFormat findDepthFormat();
    void createCubeMapImage();
    void createImageViews();
    void createSampler();
    void createRenderPass();
    void createFramebuffers();
    void createShaderModules();
    void createPipelineLayout();
    void createPipeline();

};
