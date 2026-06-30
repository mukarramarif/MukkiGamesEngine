#include "RayTracingAS.h"
#include "../Core/VkDevice.h"
#include "../CommandBufferManager.h"
#include <stdexcept>
#include <array>

RayTracingAS::RayTracingAS() = default;
RayTracingAS::~RayTracingAS() { cleanup(); }

void RayTracingAS::init(Device* device, CommandBufferManager* commandBufferManager)
{
    this->device = device;
    this->commandBufferManager = commandBufferManager;

    vkCreateAccelerationStructureKHRFunc = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(device->getDevice(), "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructureKHRFunc = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(device->getDevice(), "vkDestroyAccelerationStructureKHR"));
    vkGetAccelerationStructureBuildSizesKHRFunc = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(device->getDevice(), "vkGetAccelerationStructureBuildSizesKHR"));
    vkCmdBuildAccelerationStructuresKHRFunc = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(device->getDevice(), "vkCmdBuildAccelerationStructuresKHR"));
    vkGetAccelerationStructureDeviceAddressKHRFunc = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(device->getDevice(), "vkGetAccelerationStructureDeviceAddressKHR"));

    if (!vkCreateAccelerationStructureKHRFunc || !vkDestroyAccelerationStructureKHRFunc ||
        !vkGetAccelerationStructureBuildSizesKHRFunc || !vkCmdBuildAccelerationStructuresKHRFunc ||
        !vkGetAccelerationStructureDeviceAddressKHRFunc) {
        throw std::runtime_error("failed to load ray tracing function pointers");
    }
}

void RayTracingAS::cleanup()
{
    for (auto& blas : blases) {
        destroyAccelerationStructure(blas);
    }
    blases.clear();
    destroyAccelerationStructure(tlas);
}

void RayTracingAS::buildBLAS(const Model& model)
{
    buildBLASForModel(model);
}

void RayTracingAS::buildTLAS(const Model& model)
{
    buildTLASFromModel(model);
}

VkDeviceAddress RayTracingAS::getBufferDeviceAddress(VkBuffer buffer) const
{
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer;
    return vkGetBufferDeviceAddress(device->getDevice(), &addressInfo);
}

void RayTracingAS::createAccelerationStructureBuffer(VkDeviceSize size, AccelerationStructure& as)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device->getDevice(), &bufferInfo, nullptr, &as.buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create acceleration structure buffer!");
    }

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device->getDevice(), as.buffer, &memoryRequirements);

    VkMemoryAllocateFlagsInfo allocFlagsInfo{};
    allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = device->findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    allocInfo.pNext = &allocFlagsInfo;

    if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &as.memory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate acceleration structure buffer memory!");
    }

    vkBindBufferMemory(device->getDevice(), as.buffer, as.memory, 0);
}

void RayTracingAS::destroyAccelerationStructure(AccelerationStructure& as)
{
    if (as.handle != VK_NULL_HANDLE) {
     vkDestroyAccelerationStructureKHRFunc(device->getDevice(), as.handle, nullptr);
        as.handle = VK_NULL_HANDLE;
    }
    if (as.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device->getDevice(), as.buffer, nullptr);
        as.buffer = VK_NULL_HANDLE;
    }
    if (as.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device->getDevice(), as.memory, nullptr);
        as.memory = VK_NULL_HANDLE;
    }
    as.deviceAddress = 0;
}

void RayTracingAS::buildBLASForModel(const Model& model)
{
    if (model.vertexBuffer == VK_NULL_HANDLE || model.indexBuffer == VK_NULL_HANDLE || model.meshes.empty()) {
        return;
    }

    for (auto& blas : blases) {
        destroyAccelerationStructure(blas);
    }
    blases.clear();
    blases.resize(model.meshes.size());

    VkDeviceAddress vertexAddress = getBufferDeviceAddress(model.rtVertexBuffer != VK_NULL_HANDLE
		? model.rtVertexBuffer
		: model.vertexBuffer);
    VkDeviceAddress indexAddress = getBufferDeviceAddress(model.indexBuffer);

    for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
        const auto& mesh = model.meshes[meshIndex];
        if (mesh.primitives.empty()) {
            continue;
        }

        std::vector<VkAccelerationStructureGeometryKHR> geometries;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;
        geometries.reserve(mesh.primitives.size());
        ranges.reserve(mesh.primitives.size());

        for (const auto& primitive : mesh.primitives) {
            VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
            triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            triangles.vertexData.deviceAddress = vertexAddress;
            triangles.vertexStride = model.rtVertexBuffer != VK_NULL_HANDLE
				? sizeof(RayTracingVertex)
				: sizeof(Vertex);
            triangles.maxVertex = primitive.vertexCount > 0
                ? (primitive.firstVertex + primitive.vertexCount - 1)
                : primitive.firstVertex;
            triangles.indexType = VK_INDEX_TYPE_UINT32;
            triangles.indexData.deviceAddress = indexAddress + static_cast<VkDeviceAddress>(primitive.firstIndex) * sizeof(uint32_t);
            triangles.transformData.deviceAddress = 0;

            VkAccelerationStructureGeometryKHR geometry{};
            geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
            geometry.geometry.triangles = triangles;
            geometries.push_back(geometry);

            VkAccelerationStructureBuildRangeInfoKHR range{};
            range.firstVertex = 0;
            range.primitiveOffset = 0;
            range.primitiveCount = primitive.indexCount / 3;
            range.transformOffset = 0;
            ranges.push_back(range);
        }

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.geometryCount = static_cast<uint32_t>(geometries.size());
        buildInfo.pGeometries = geometries.data();

        std::vector<uint32_t> maxPrimCounts;
        maxPrimCounts.reserve(ranges.size());
        for (const auto& range : ranges) {
            maxPrimCounts.push_back(range.primitiveCount);
        }

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
        sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHRFunc(
            device->getDevice(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo,
            maxPrimCounts.data(),
            &sizeInfo);

        AccelerationStructure& blas = blases[meshIndex];
        createAccelerationStructureBuffer(sizeInfo.accelerationStructureSize, blas);

        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        createInfo.size = sizeInfo.accelerationStructureSize;
        createInfo.buffer = blas.buffer;

      if (vkCreateAccelerationStructureKHRFunc(device->getDevice(), &createInfo, nullptr, &blas.handle) != VK_SUCCESS) {
            throw std::runtime_error("failed to create BLAS!");
        }

        VkBuffer scratchBuffer = VK_NULL_HANDLE;
        VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
        device->createBuffer(
            sizeInfo.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            scratchBuffer,
            scratchMemory);

        VkDeviceAddress scratchAddress = getBufferDeviceAddress(scratchBuffer);

        buildInfo.dstAccelerationStructure = blas.handle;
        buildInfo.scratchData.deviceAddress = scratchAddress;

        std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> rangePtrs;
        rangePtrs.reserve(ranges.size());
        for (const auto& range : ranges) {
            rangePtrs.push_back(&range);
        }

        VkCommandBuffer commandBuffer = commandBufferManager->beginSingleTimeCommands();
        vkCmdBuildAccelerationStructuresKHRFunc(commandBuffer, 1, &buildInfo, rangePtrs.data());
        commandBufferManager->endSingleTimeCommands(commandBuffer);

        vkDestroyBuffer(device->getDevice(), scratchBuffer, nullptr);
        vkFreeMemory(device->getDevice(), scratchMemory, nullptr);

        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure = blas.handle;
     blas.deviceAddress = vkGetAccelerationStructureDeviceAddressKHRFunc(device->getDevice(), &addressInfo);
    }
}

void RayTracingAS::buildTLASFromModel(const Model& model)
{
    if (blases.empty()) {
        return;
    }

    destroyAccelerationStructure(tlas);

    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.reserve(model.nodes.size());

    for (const auto& node : model.nodes) {
        if (node.meshIndex < 0 || node.meshIndex >= static_cast<int32_t>(blases.size())) {
            continue;
        }
        const auto& blas = blases[static_cast<size_t>(node.meshIndex)];
        if (blas.deviceAddress == 0) {
            continue;
        }

        VkTransformMatrixKHR transform{};
        auto matrix = glm::mat3x4(glm::transpose(node.worldTransform));
        memcpy(transform.matrix, &matrix, sizeof(matrix));
        VkAccelerationStructureInstanceKHR instance{};
        instance.transform = transform;
		instance.instanceCustomIndex = static_cast<uint32_t>(node.meshIndex);
        instance.mask = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = blas.deviceAddress;
        instances.push_back(instance);
    }

    if (instances.empty()) {
        return;
    }

    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceMemory = VK_NULL_HANDLE;
    VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * instances.size();

    device->createBuffer(
        instanceBufferSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        instanceBuffer,
        instanceMemory);

    void* mappedData = nullptr;
    vkMapMemory(device->getDevice(), instanceMemory, 0, instanceBufferSize, 0, &mappedData);
    memcpy(mappedData, instances.data(), static_cast<size_t>(instanceBufferSize));
    vkUnmapMemory(device->getDevice(), instanceMemory);

    VkDeviceAddress instanceAddress = getBufferDeviceAddress(instanceBuffer);

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.geometry.instances.data.deviceAddress = instanceAddress;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = static_cast<uint32_t>(instances.size());

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    uint32_t primitiveCount = range.primitiveCount;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHRFunc(
        device->getDevice(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo);

    createAccelerationStructureBuffer(sizeInfo.accelerationStructureSize, tlas);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.buffer = tlas.buffer;

    if (vkCreateAccelerationStructureKHRFunc(device->getDevice(), &createInfo, nullptr, &tlas.handle) != VK_SUCCESS) {
        throw std::runtime_error("failed to create TLAS!");
    }

    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
    device->createBuffer(
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        scratchBuffer,
        scratchMemory);

    VkDeviceAddress scratchAddress = getBufferDeviceAddress(scratchBuffer);
    buildInfo.dstAccelerationStructure = tlas.handle;
    buildInfo.scratchData.deviceAddress = scratchAddress;

    const VkAccelerationStructureBuildRangeInfoKHR* rangePtr = &range;
    VkCommandBuffer commandBuffer = commandBufferManager->beginSingleTimeCommands();
    vkCmdBuildAccelerationStructuresKHRFunc(commandBuffer, 1, &buildInfo, &rangePtr);
    commandBufferManager->endSingleTimeCommands(commandBuffer);

    vkDestroyBuffer(device->getDevice(), instanceBuffer, nullptr);
    vkFreeMemory(device->getDevice(), instanceMemory, nullptr);
    vkDestroyBuffer(device->getDevice(), scratchBuffer, nullptr);
    vkFreeMemory(device->getDevice(), scratchMemory, nullptr);

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = tlas.handle;
    tlas.deviceAddress = vkGetAccelerationStructureDeviceAddressKHRFunc(device->getDevice(), &addressInfo);
}
