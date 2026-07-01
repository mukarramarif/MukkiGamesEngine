#include "RayTracingPipeline.h"
#include "../Core/VkDevice.h"
#include "../utils/utils.h"
#include <stdexcept>
#include <array>

RayTracingPipeline::RayTracingPipeline() = default;
RayTracingPipeline::~RayTracingPipeline() { cleanup(); }

void RayTracingPipeline::init(Device* device)
{
    this->device = device;
    vkCmdTraceRaysKHRFunc = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(device->getDevice(), "vkCmdTraceRaysKHR"));
    if (!vkCmdTraceRaysKHRFunc) {
        throw std::runtime_error("failed to load vkCmdTraceRaysKHR");
    }
}

void RayTracingPipeline::cleanup()
{
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device->getDevice(), pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device->getDevice(), pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    if (sbt.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device->getDevice(), sbt.buffer, nullptr);
        sbt.buffer = VK_NULL_HANDLE;
    }
    if (sbt.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device->getDevice(), sbt.memory, nullptr);
        sbt.memory = VK_NULL_HANDLE;
    }
    sbt.deviceAddress = 0;
}

VkDeviceAddress RayTracingPipeline::getBufferDeviceAddress(VkBuffer buffer) const
{
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer;
    return vkGetBufferDeviceAddress(device->getDevice(), &addressInfo);
}

VkShaderModule RayTracingPipeline::createShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create ray tracing shader module!");
    }

    return shaderModule;
}

void RayTracingPipeline::createPipeline(VkDescriptorSetLayout descriptorSetLayout)
{
    auto rgenCode = EngineUtils::readFile("Shaders/rt.rgen.spv");
    auto rmissCode = EngineUtils::readFile("Shaders/rt.rmiss.spv");
    auto rchitCode = EngineUtils::readFile("Shaders/rt.rchit.spv");

    VkShaderModule rgenModule = createShaderModule(rgenCode);
    VkShaderModule rmissModule = createShaderModule(rmissCode);
    VkShaderModule rchitModule = createShaderModule(rchitCode);

    VkPipelineShaderStageCreateInfo rgenStage{};
    rgenStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    rgenStage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    rgenStage.module = rgenModule;
    rgenStage.pName = "main";

    VkPipelineShaderStageCreateInfo rmissStage{};
    rmissStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    rmissStage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    rmissStage.module = rmissModule;
    rmissStage.pName = "main";

    VkPipelineShaderStageCreateInfo rchitStage{};
    rchitStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    rchitStage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    rchitStage.module = rchitModule;
    rchitStage.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 3> stages{ rgenStage, rmissStage, rchitStage };

    VkRayTracingShaderGroupCreateInfoKHR rgenGroup{};
    rgenGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    rgenGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    rgenGroup.generalShader = 0;
    rgenGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    rgenGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    rgenGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    VkRayTracingShaderGroupCreateInfoKHR rmissGroup{};
    rmissGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    rmissGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    rmissGroup.generalShader = 1;
    rmissGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    rmissGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    rmissGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    VkRayTracingShaderGroupCreateInfoKHR rchitGroup{};
    rchitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    rchitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    rchitGroup.generalShader = VK_SHADER_UNUSED_KHR;
    rchitGroup.closestHitShader = 2;
    rchitGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    rchitGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    std::array<VkRayTracingShaderGroupCreateInfoKHR, 3> groups{ rgenGroup, rmissGroup, rchitGroup };

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device->getDevice(), &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create ray tracing pipeline layout!");
    }

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
    pipelineInfo.pGroups = groups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 1;
    pipelineInfo.layout = pipelineLayout;

    auto vkCreateRayTracingPipelinesKHRFunc = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(device->getDevice(), "vkCreateRayTracingPipelinesKHR"));
    if (!vkCreateRayTracingPipelinesKHRFunc) {
        throw std::runtime_error("failed to load vkCreateRayTracingPipelinesKHR");
    }

    if (vkCreateRayTracingPipelinesKHRFunc(device->getDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create ray tracing pipeline!");
    }

    vkDestroyShaderModule(device->getDevice(), rgenModule, nullptr);
    vkDestroyShaderModule(device->getDevice(), rmissModule, nullptr);
    vkDestroyShaderModule(device->getDevice(), rchitModule, nullptr);
}

void RayTracingPipeline::createShaderBindingTable()
{
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{};
    rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 properties2{};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &rtProperties;
    vkGetPhysicalDeviceProperties2(device->getPhysicalDevice(), &properties2);

    sbt.handleSize = rtProperties.shaderGroupHandleSize;
    sbt.handleSizeAligned = (rtProperties.shaderGroupHandleSize + rtProperties.shaderGroupHandleAlignment - 1) & ~(rtProperties.shaderGroupHandleAlignment - 1);
 sbt.baseAlignment = rtProperties.shaderGroupBaseAlignment;
    sbt.groupCount = 3;

  uint32_t sbtStride = (sbt.handleSizeAligned + sbt.baseAlignment - 1) & ~(sbt.baseAlignment - 1);
    uint32_t sbtSize = sbt.groupCount * sbtStride;
    std::vector<uint8_t> shaderHandleStorage(sbtSize);

    auto vkGetRayTracingShaderGroupHandlesKHRFunc = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(device->getDevice(), "vkGetRayTracingShaderGroupHandlesKHR"));
    if (!vkGetRayTracingShaderGroupHandlesKHRFunc) {
        throw std::runtime_error("failed to load vkGetRayTracingShaderGroupHandlesKHR");
    }

    std::vector<uint8_t> handleData(sbt.groupCount * sbt.handleSize);
    if (vkGetRayTracingShaderGroupHandlesKHRFunc(device->getDevice(), pipeline, 0, sbt.groupCount, static_cast<uint32_t>(handleData.size()), handleData.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to get shader group handles");
    }

    for (uint32_t group = 0; group < sbt.groupCount; ++group) {
        memcpy(shaderHandleStorage.data() + group * sbtStride, handleData.data() + group * sbt.handleSize, sbt.handleSize);
    }

    device->createBuffer(
        sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        sbt.buffer,
        sbt.memory);

    void* mapped = nullptr;
    vkMapMemory(device->getDevice(), sbt.memory, 0, sbtSize, 0, &mapped);
    memcpy(mapped, shaderHandleStorage.data(), sbtSize);
    vkUnmapMemory(device->getDevice(), sbt.memory);

    sbt.deviceAddress = getBufferDeviceAddress(sbt.buffer);
}
