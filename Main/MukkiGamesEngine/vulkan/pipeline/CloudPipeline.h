#pragma once
#include <vulkan/vulkan.h>
#include "../Core/VkDevice.h"
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
class Device;

struct PC {
    float time;
};

class CloudPipeline {
public:

    CloudPipeline();
    CloudPipeline(const CloudPipeline &) = default;
    CloudPipeline(CloudPipeline &&) = delete;
    CloudPipeline &operator=(const CloudPipeline &) = default;
    CloudPipeline &operator=(CloudPipeline &&) = delete;
    ~CloudPipeline();

    void createCloudPipeline(Device* device, const std::string& computeShaderPath);
    void createDescriptorSetLayout(Device* device);
    void createDescriptorPool(Device* device, uint32_t maxSets=1);
    void createDescriptorSets(Device* device, VkDescriptorPool descriptorPool, VkImageView cloudImageView, VkSampler cloudSampler, uint32_t setCount);
    void resetDescriptorPool(Device* device);
    void cleanup(Device* device);

    [[nodiscard]] VkPipeline getPipeline() const { return computePipeline; }
    [[nodiscard]] VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
    [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }
private:
    VkShaderModule createShaderModule(Device* device, const std::vector<char>& code);

	VkPipeline computePipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet computeDescriptorSets;

};
