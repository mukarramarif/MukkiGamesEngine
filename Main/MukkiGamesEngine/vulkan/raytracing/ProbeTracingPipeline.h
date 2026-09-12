#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "RayTracingPipeline.h"  // for ShaderBindingTable

class Device;

// Minimal ray tracing pipeline for the probe update pass:
//   [0] probeTrace.rgen  (1 thread per probe)
//   [1] probe.rmiss
//   [2] probe.rchit      (triangles hit group, closest-hit only - no any-hit)
class ProbeTracingPipeline {
public:
    ProbeTracingPipeline();
    ~ProbeTracingPipeline();

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
