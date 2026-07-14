#include "VulkanRenderer.h"
#include "../vulkan/Core/EngineWindow.h"
#include "../vulkan/Core/VkDevice.h"
#include "../vulkan/Core/VkInstance.h"
#include "../vulkan/Core/SwapChain.h"
#include "../vulkan/Core/ShaderCompiler.h"
#include "../vulkan/RenderPass.h"
#include "../vulkan/pipeline.h"
#include "../vulkan/CommandBufferManager.h"
#include "../vulkan/Resources/TextureManager.h"
#include "../vulkan/Resources/BufferManager.h"
#include "../vulkan/Resources/ObjectLoader.h"
#include "../vulkan/Resources/SkyBox.h"
#include "../vulkan/Resources/ShadowMap.h"
#include "../vulkan/Resources/DeletionQueue.h"
#include "../vulkan/Descriptors/VkDescriptor.h"
#include "../vulkan/objects/vertex.h"
#include "../vulkan/objects/lights.h"
#include "../vulkan/objects/UBO.h"
#include "../vulkan/pipeline/computePipeline.h"
#include "../vulkan/raytracing/RayTracingAS.h"
#include "../vulkan/raytracing/RayTracingPipeline.h"
#include <stdexcept>
#include <array>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

static const std::vector<Vertex> s_vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
};
static const std::vector<uint32_t> s_indices = { 0, 1, 2, 2, 3, 0 };

// ----- VulkanRenderer -----

VulkanRenderer::VulkanRenderer() : renderPass(VK_NULL_HANDLE), currentFrame(0) {}
VulkanRenderer::~VulkanRenderer() { shutdown(); }

void VulkanRenderer::init(const RenderConfig& config)
{
    ShaderCompiler::compileShadersIfNeeded();

    window = std::make_unique<EngineWindow>();
    window->init(config.windowWidgth, config.windowHeight, config.windowTitle.c_str());

    Instance instance;
    instance.createInstance();
    VkSurfaceKHR surface = window->createSurface(instance.getInstance());
    device = std::make_unique<Device>(instance, surface);

    swapChain = std::make_unique<VulkanSwap>();
    swapChain->initSwap(*device, surface, window->getGLFWwindow());
    swapChain->createImageViews();

    shadowMap = std::make_unique<ShadowMap>();
    shadowMap->init(device.get(), 2048);

    renderPassObj = std::make_unique<VulkanRenderPass>(device.get(), swapChain->getSwapChainImageFormat());
    renderPass = renderPassObj->getRenderPass();

    cmdBufManager = std::make_unique<CommandBufferManager>();
    cmdBufManager->init(device.get(), MAX_FRAMES_IN_FLIGHT);

    bufferManager = std::make_unique<BufferManager>();
    bufferManager->init(*device, *cmdBufManager);

    textureManager = std::make_unique<TextureManager>();
    textureManager->init(*device, *cmdBufManager, *bufferManager);

    objectLoader = std::make_unique<ObjectLoader>();
    objectLoader->init(device.get(), textureManager.get(), bufferManager.get());

    VkExtent2D extent = swapChain->getSwapChainExtent();
    VkImage depthImg; VkDeviceMemory depthMem; VkImageView depthView;
    textureManager->createdepthResources(depthImg, depthMem, depthView, extent.width, extent.height);
    swapChain->createFramebuffers(renderPass, depthView);

    // Descriptor set layout
    {
        VkDescriptorSetLayoutBinding bindings[4] = {};
        bindings[0].binding = 0; bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1; bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].binding = 1; bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount = 1; bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[2].binding = 2; bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[2].descriptorCount = 1; bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[3].binding = 3; bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[3].descriptorCount = 1; bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 4; layoutInfo.pBindings = bindings;
        if (vkCreateDescriptorSetLayout(device->getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor set layout!");
    }

    // Pipeline layout
    {
        VkPipelineLayoutCreateInfo plInfo{};
        plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plInfo.setLayoutCount = 1; plInfo.pSetLayouts = &descriptorSetLayout;
        if (vkCreatePipelineLayout(device->getDevice(), &plInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create pipeline layout!");
    }

    // Graphics pipeline
    {
        PipelineConfigInfo pipelineConfig{};
        VulkanPipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.renderPass = renderPass;
        pipelineConfig.pipelineLayout = pipelineLayout;
        VulkanPipeline::enableAlphaBlending(pipelineConfig);
        graphicsPipeline = std::make_unique<VulkanPipeline>(
            device.get(), "Shaders/shader.vert.spv", "Shaders/brdf.frag.spv", pipelineConfig);

        PipelineConfigInfo additiveConfig{};
        VulkanPipeline::defaultPipelineConfigInfo(additiveConfig);
        additiveConfig.renderPass = renderPass;
        additiveConfig.pipelineLayout = pipelineLayout;
        VulkanPipeline::enableAdditiveBlending(additiveConfig);
        additivePipeline = std::make_unique<VulkanPipeline>(
            device.get(), "Shaders/shader.vert.spv", "Shaders/brdf.frag.spv", additiveConfig);
    }

    // Sync objects
    {
        size_t imageCount = swapChain->getSwapChainImages().size();
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(imageCount);
        imagesInFlight.resize(imageCount, VK_NULL_HANDLE);
        swapChainImageLayouts.assign(imageCount, VK_IMAGE_LAYOUT_UNDEFINED);

        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkDevice vkDev = device->getDevice();
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkCreateSemaphore(vkDev, &semInfo, nullptr, &imageAvailableSemaphores[i]);
            vkCreateFence(vkDev, &fenceInfo, nullptr, &inFlightFences[i]);
        }
        for (size_t i = 0; i < imageCount; i++)
            vkCreateSemaphore(vkDev, &semInfo, nullptr, &renderFinishedSemaphores[i]);
    }

    // Fallback buffers
    {
        VkDeviceSize vbSize = sizeof(s_vertices[0]) * s_vertices.size();
        VkBuffer staging; VkDeviceMemory stagingMem;
        device->createBuffer(vbSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
        void* data; vkMapMemory(device->getDevice(), stagingMem, 0, vbSize, 0, &data);
        memcpy(data, s_vertices.data(), (size_t)vbSize); vkUnmapMemory(device->getDevice(), stagingMem);
        VkBuffer vb; VkDeviceMemory vbMem;
        device->createBuffer(vbSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vb, vbMem);
        device->copyBuffer(staging, vb, vbSize);
        vkDestroyBuffer(device->getDevice(), staging, nullptr);
        vkFreeMemory(device->getDevice(), stagingMem, nullptr);
        // Store in member vars - need to add to header
        // For now: stored locally, fallback not used for now
        if (staging != VK_NULL_HANDLE) {}
    }

    // Uniform buffers (shared)
    {
        VkDeviceSize bufSize = sizeof(UniformBufferObject);
        sharedUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        sharedUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        sharedUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            device->createBuffer(bufSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                sharedUniformBuffers[i], sharedUniformBuffersMemory[i]);
            vkMapMemory(device->getDevice(), sharedUniformBuffersMemory[i], 0, bufSize, 0, &sharedUniformBuffersMapped[i]);
        }
    }

    // Default material UBO
    {
        VkDeviceSize bufSize = sizeof(MaterialUBO);
        defaultMaterialUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        defaultMaterialUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        defaultMaterialUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
        MaterialUBO defaultMat{};
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            device->createBuffer(bufSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                defaultMaterialUniformBuffers[i], defaultMaterialUniformBuffersMemory[i]);
            vkMapMemory(device->getDevice(), defaultMaterialUniformBuffersMemory[i], 0, bufSize, 0, &defaultMaterialUniformBuffersMapped[i]);
            memcpy(defaultMaterialUniformBuffersMapped[i], &defaultMat, sizeof(MaterialUBO));
        }
    }

    // Texture resources
    {
        VkImage tImg; VkDeviceMemory tMem; VkImageView tView; VkSampler tSampler;
        textureManager->createDebugTextureImage(tImg, tMem, tView);
        textureManager->createTextureSampler(tSampler);
    }

    // Descriptor sets
    {
        descriptorBoss = std::make_unique<VkDescriptorBoss>(device.get(), MAX_FRAMES_IN_FLIGHT);
        descriptorBoss->createDescriptorPool(MAX_FRAMES_IN_FLIGHT);
        descriptorBoss->createDescriptorSets(descriptorSetLayout, MAX_FRAMES_IN_FLIGHT, defaultDescriptorSets);
    }

    initialized = true;
}

void VulkanRenderer::shutdown()
{
    if (!initialized) return;
    initialized = false;
}

void VulkanRenderer::beginFrame() {}
void VulkanRenderer::endFrame() {}

void VulkanRenderer::addObject(uint32_t id, const ::Model& model, const glm::mat4& transform)
{
    // Remove existing if present
    auto it = objects.find(id);
    if (it != objects.end()) { removeObject(id); }

    ObjectGPUData data;
    data.model = const_cast<::Model*>(&model); // non-owning reference

    VkDeviceSize bufSize = sizeof(UniformBufferObject);
    data.uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    data.uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    data.uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        device->createBuffer(bufSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            data.uniformBuffers[i], data.uniformBuffersMemory[i]);
        vkMapMemory(device->getDevice(), data.uniformBuffersMemory[i], 0, bufSize, 0, &data.uniformBuffersMapped[i]);
    }

    size_t matCount = model.materials.empty() ? 1 : model.materials.size();
    if (!model.materials.empty()) {
        data.materialUniformBuffers.resize(matCount);
        data.materialUniformBuffersMemory.resize(matCount);
        data.materialUniformBuffersMapped.resize(matCount);
        for (size_t m = 0; m < matCount; m++) {
            data.materialUniformBuffers[m].resize(MAX_FRAMES_IN_FLIGHT);
            data.materialUniformBuffersMemory[m].resize(MAX_FRAMES_IN_FLIGHT);
            data.materialUniformBuffersMapped[m].resize(MAX_FRAMES_IN_FLIGHT);
            MaterialUBO matData{};
            matData.metallicFactor = model.materials[m].metallicFactor;
            matData.roughnessFactor = model.materials[m].roughnessFactor;
            for (size_t f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
                device->createBuffer(sizeof(MaterialUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    data.materialUniformBuffers[m][f], data.materialUniformBuffersMemory[m][f]);
                vkMapMemory(device->getDevice(), data.materialUniformBuffersMemory[m][f], 0, sizeof(MaterialUBO), 0, &data.materialUniformBuffersMapped[m][f]);
                memcpy(data.materialUniformBuffersMapped[m][f], &matData, sizeof(MaterialUBO));
            }
        }
    }

    data.descriptorSets.resize(matCount);
    for (size_t m = 0; m < matCount; m++) {
        data.descriptorSets[m].resize(MAX_FRAMES_IN_FLIGHT);
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorBoss->getDescriptorPool();
        allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        allocInfo.pSetLayouts = layouts.data();
        if (vkAllocateDescriptorSets(device->getDevice(), &allocInfo, data.descriptorSets[m].data()) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor sets!");

        const auto& mat = (m < model.materials.size()) ? model.materials[m] : ::Material{};
        for (size_t f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
            std::vector<VkWriteDescriptorSet> writes;

            VkDescriptorBufferInfo bufInfo{ data.uniformBuffers[f], 0, sizeof(UniformBufferObject) };
            VkWriteDescriptorSet uboWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            uboWrite.dstSet = data.descriptorSets[m][f];
            uboWrite.dstBinding = 0; uboWrite.descriptorCount = 1;
            uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboWrite.pBufferInfo = &bufInfo;
            writes.push_back(uboWrite);

            VkDescriptorImageInfo imgInfo{};
            imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            if (mat.baseColorTextureIndex >= 0 && mat.baseColorTextureIndex < (int)model.textures.size()) {
                imgInfo.imageView = model.textures[mat.baseColorTextureIndex].imageView;
                imgInfo.sampler = model.textures[mat.baseColorTextureIndex].sampler;
            } else {
                imgInfo.imageView = VK_NULL_HANDLE;
                imgInfo.sampler = VK_NULL_HANDLE;
            }
            VkWriteDescriptorSet texWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            texWrite.dstSet = data.descriptorSets[m][f];
            texWrite.dstBinding = 1; texWrite.descriptorCount = 1;
            texWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            texWrite.pImageInfo = &imgInfo;
            writes.push_back(texWrite);

            VkDescriptorBufferInfo matBufInfo{};
            matBufInfo.buffer = data.materialUniformBuffers.empty() ?
                defaultMaterialUniformBuffers[f] : data.materialUniformBuffers[m][f];
            matBufInfo.offset = 0; matBufInfo.range = sizeof(MaterialUBO);
            VkWriteDescriptorSet matWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            matWrite.dstSet = data.descriptorSets[m][f];
            matWrite.dstBinding = 2; matWrite.descriptorCount = 1;
            matWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            matWrite.pBufferInfo = &matBufInfo;
            writes.push_back(matWrite);

            VkDescriptorImageInfo shadowInfo{};
            shadowInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            if (shadowMap) {
                shadowInfo.imageView = shadowMap->getShadowMapImageView();
                shadowInfo.sampler = shadowMap->getShadowSampler();
            } else {
                shadowInfo.imageView = VK_NULL_HANDLE;
                shadowInfo.sampler = VK_NULL_HANDLE;
            }
            VkWriteDescriptorSet shadowWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            shadowWrite.dstSet = data.descriptorSets[m][f];
            shadowWrite.dstBinding = 3; shadowWrite.descriptorCount = 1;
            shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            shadowWrite.pImageInfo = &shadowInfo;
            writes.push_back(shadowWrite);

            vkUpdateDescriptorSets(device->getDevice(), (uint32_t)writes.size(), writes.data(), 0, nullptr);
        }
    }

    objects[id] = std::move(data);
}

void VulkanRenderer::removeObject(uint32_t id)
{
    auto it = objects.find(id);
    if (it == objects.end()) return;
    auto& data = it->second;
    for (size_t i = 0; i < data.uniformBuffers.size(); i++) {
        if (data.uniformBuffers[i] != VK_NULL_HANDLE) vkDestroyBuffer(device->getDevice(), data.uniformBuffers[i], nullptr);
        if (data.uniformBuffersMemory[i] != VK_NULL_HANDLE) vkFreeMemory(device->getDevice(), data.uniformBuffersMemory[i], nullptr);
    }
    for (size_t m = 0; m < data.materialUniformBuffers.size(); m++) {
        for (size_t f = 0; f < data.materialUniformBuffers[m].size(); f++) {
            if (data.materialUniformBuffers[m][f] != VK_NULL_HANDLE) vkDestroyBuffer(device->getDevice(), data.materialUniformBuffers[m][f], nullptr);
            if (data.materialUniformBuffersMemory[m][f] != VK_NULL_HANDLE) vkFreeMemory(device->getDevice(), data.materialUniformBuffersMemory[m][f], nullptr);
        }
    }
    objects.erase(it);
}

void VulkanRenderer::setLights(const std::vector<Light>& lights, float ambientStrength)
{
    mLights = lights;
    mAmbientStrength = ambientStrength;
}

void VulkanRenderer::setCamera(const Camera& camera) {}
void VulkanRenderer::setSkybox(const std::string& skyBoxPath) {}

void VulkanRenderer::setRenderMode(Mode mode) { mRenderMode = mode; }
void VulkanRenderer::getRenderMode() const {}

void* VulkanRenderer::getNativeWindow() const { return window ? (void*)window->getGLFWwindow() : nullptr; }
void* VulkanRenderer::getNativeCommandBuffer() { return (void*)currentCmdBuffer; }
void* VulkanRenderer::getNativeDevice() { return device ? (void*)device->getDevice() : nullptr; }

class TextureManager* VulkanRenderer::getTextureManager() { return textureManager.get(); }
class BufferManager* VulkanRenderer::getBufferManager() { return bufferManager.get(); }

Device* VulkanRenderer::getDevicePtr() { return device.get(); }
VulkanSwap* VulkanRenderer::getSwapChainPtr() { return swapChain.get(); }
VulkanRenderPass* VulkanRenderer::getRenderPassPtr() { return renderPassObj.get(); }
CommandBufferManager* VulkanRenderer::getCmdBufManagerPtr() { return cmdBufManager.get(); }
VkRenderPass VulkanRenderer::getRenderPass() { return renderPass; }
ShadowMap* VulkanRenderer::getShadowMapPtr() { return shadowMap.get(); }

void VulkanRenderer::onResize(int width, int height) {}
float VulkanRenderer::getFrameTime() const { return mLastFrameTime; }
uint32_t VulkanRenderer::getFrameCount() const { return mFrameCount; }
