#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

class Device;

struct CloudPushConstants {
    float iResolution[2];
    float iTime;
    float sunDirX, sunDirY, sunDirZ;
    float cloudBase;
    float cloudThickness;
    // camera Transforms
    float camPosX;
    float camPosY;
    float camPosZ;
    float camFwdX;
    float camFwdY;
    float camFwdZ;
    float camUpX;
    float camUpY;
    float camUpZ;
    float camRightX;
    float camRightY;
    float camRightZ;
};

class CloudPipeline
{
public:
    CloudPipeline();
    ~CloudPipeline();

    void createCloudPipeline(Device* device, const std::string& computeShaderPath);
    void createDescriptorSetLayout(Device* device);
    void createDescriptorPool(Device* device, uint32_t maxSets);
    void createDescriptorSets(Device* device, VkImageView outputImageView,
                              VkImageView sceneColorImageView, VkSampler sceneColorSampler,
                              VkImageView depthImageView, VkSampler depthSampler,
                              VkImageView noiseImageView, VkSampler noiseSampler,
                              VkImageView weatherImageView, VkSampler weatherSampler);
    void resetDescriptorPool(Device* device);
    void cleanup(Device* device);

    VkPipeline       getPipeline()       const { return pipeline;        }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout;  }
    VkDescriptorSet  getDescriptorSet()  const { return descriptorSet;   }

private:
    VkShaderModule createShaderModule(Device* device, const std::vector<char>& code);

    VkPipeline            pipeline            = VK_NULL_HANDLE;
    VkPipelineLayout      pipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       descriptorSet       = VK_NULL_HANDLE;
};
