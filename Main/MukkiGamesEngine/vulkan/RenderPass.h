#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <vector>
#include "Core/VkDevice.h"
#include "Core/VkInstance.h"

class VulkanRenderPass {
public:
    VulkanRenderPass(Device* device, VkFormat swapChainImageFormat);
    ~VulkanRenderPass();

    // Prevent copying
    VulkanRenderPass(const VulkanRenderPass&) = delete;
    VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;

    VkRenderPass getRenderPass() const { return renderPass; }

private:
    Device* device;
    VkRenderPass renderPass;

    void createRenderPass(VkFormat swapChainImageFormat);
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    );
};