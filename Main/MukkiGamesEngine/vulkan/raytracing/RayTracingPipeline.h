#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class Device;

struct ShaderBindingTable {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceAddress deviceAddress = 0;
    uint32_t handleSize = 0;
 uint32_t handleSizeAligned = 0;
    uint32_t baseAlignment = 0;
    uint32_t groupCount = 0;
};

class RayTracingPipeline {
public:
    RayTracingPipeline();
    ~RayTracingPipeline();

    void init(Device* device);
    void cleanup();

    void createPipeline(VkDescriptorSetLayout descriptorSetLayout);
    void createShaderBindingTable();

    VkPipeline getPipeline() const { return pipeline; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
    const ShaderBindingTable& getSbt() const { return sbt; }

    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHRFunc = nullptr;

private:
    Device* device = nullptr;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    ShaderBindingTable sbt{};

    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer) const;
    VkShaderModule createShaderModule(const std::vector<char>& code);
};
