#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "../Resources/ObjectLoader.h"

class Device;
class CommandBufferManager;

struct AccelerationStructure {
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint64_t deviceAddress = 0;
};

class RayTracingAS {
public:
    RayTracingAS();
    ~RayTracingAS();

    void init(Device* device, CommandBufferManager* commandBufferManager);
    void cleanup();

    void buildBLAS(const Model& model);
    void buildTLAS(const Model& model);

    const AccelerationStructure& getTLAS() const { return tlas; }

private:
    Device* device = nullptr;
    CommandBufferManager* commandBufferManager = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHRFunc = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHRFunc = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHRFunc = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHRFunc = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHRFunc = nullptr;

    std::vector<AccelerationStructure> blases;
    AccelerationStructure tlas{};

    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer) const;
    void createAccelerationStructureBuffer(VkDeviceSize size, AccelerationStructure& as);
    void destroyAccelerationStructure(AccelerationStructure& as);
    void buildBLASForModel(const Model& model);
    void buildTLASFromModel(const Model& model);
};
