#include "ShadowCubeMap.h"
#include "../utils/utils.h"
#include "../objects/vertex.h"
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
const glm::vec3 ShadowCubeMap::faceDirections[6] = {
    glm::vec3( 1.0f,  0.0f,  0.0f),  // +X
    glm::vec3(-1.0f,  0.0f,  0.0f),  // -X
    glm::vec3( 0.0f,  1.0f,  0.0f),  // +Y
    glm::vec3( 0.0f, -1.0f,  0.0f),  // -Y
    glm::vec3( 0.0f,  0.0f,  1.0f),  // +Z
    glm::vec3( 0.0f,  0.0f, -1.0f),  // -Z
};

const glm::vec3 ShadowCubeMap::faceUps[6] = {
    glm::vec3(0.0f, -1.0f,  0.0f),  // +X
    glm::vec3(0.0f, -1.0f,  0.0f),  // -X
    glm::vec3(0.0f,  0.0f,  1.0f),  // +Y
    glm::vec3(0.0f,  0.0f, -1.0f),  // -Y
    glm::vec3(0.0f, -1.0f,  0.0f),  // +Z
    glm::vec3(0.0f, -1.0f,  0.0f),  // -Z
};

ShadowCubeMap::ShadowCubeMap() = default;
ShadowCubeMap::~ShadowCubeMap() { cleanup(); }

bool ShadowCubeMap::init(Device* deviceIn, uint32_t size, float farPlaneIn)
{
    device = deviceIn;
    cubeMapSize = size;
    farPlane = farPlaneIn;

    createCubeMapImage();
    createImageViews();
    createSampler();
    createRenderPass();
    createFramebuffers();
    createShaderModules();
    createPipelineLayout();
    createPipeline();
    return true;
}

glm::mat4 ShadowCubeMap::computeFaceViewProj(uint32_t faceIndex,
                                             const glm::vec3& lightPos,
                                             float farPlane)
{
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, farPlane);
    proj[1][1] *= -1.0f; // Vulkan clip-space Y flip
    return proj * glm::lookAt(lightPos,
                              lightPos + faceDirections[faceIndex],
                              faceUps[faceIndex]);
}

void ShadowCubeMap::BindForWriting(VkCommandBuffer cmd, uint32_t faceIndex)
{
    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = cubeMapRenderPass;
    rpInfo.framebuffer = faceFramebuffers[faceIndex];
    rpInfo.renderArea.offset = { 0, 0 };
    rpInfo.renderArea.extent = { cubeMapSize, cubeMapSize };

    VkClearValue clearValue{};
    clearValue.depthStencil = { 1.0f, 0 };
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cubeMapPipeline);
}

// ─────────────────────────────────────────────────────────────
//  Resource creation
// ─────────────────────────────────────────────────────────────

VkFormat ShadowCubeMap::findDepthFormat()
{
    std::vector<VkFormat> candidates = {
        VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT
    };
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(device->getPhysicalDevice(), format, &props);
        if ((props.optimalTilingFeatures &
             (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
              VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) ==
            (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
             VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported depth format for cube shadow map!");
}

void ShadowCubeMap::createCubeMapImage()
{
    VkFormat depthFormat = findDepthFormat();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { cubeMapSize, cubeMapSize, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 6;
    imageInfo.format = depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(device->getDevice(), &imageInfo, nullptr, &cubeMapImage) != VK_SUCCESS)
        throw std::runtime_error("failed to create shadow cube map image!");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device->getDevice(), cubeMapImage, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex =
        device->findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &cubeMapImageMemory) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate shadow cube map memory!");
    vkBindImageMemory(device->getDevice(), cubeMapImage, cubeMapImageMemory, 0);
}

void ShadowCubeMap::createImageViews()
{
    // One CUBE view covering all 6 layers — used for sampling in the main pass
    VkImageViewCreateInfo cubeViewInfo{};
    cubeViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    cubeViewInfo.image = cubeMapImage;
    cubeViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    cubeViewInfo.format = findDepthFormat();
    cubeViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;   // ← was COLOR, fixed
    cubeViewInfo.subresourceRange.baseMipLevel = 0;
    cubeViewInfo.subresourceRange.levelCount = 1;
    cubeViewInfo.subresourceRange.baseArrayLayer = 0;
    cubeViewInfo.subresourceRange.layerCount = 6;
    if (vkCreateImageView(device->getDevice(), &cubeViewInfo, nullptr, &cubeMapImageView) != VK_SUCCESS)
        throw std::runtime_error("failed to create cube shadow map view!");

    // Six 2D views (one layer each) — used as depth attachments per face
    for (uint32_t face = 0; face < 6; face++) {
        VkImageViewCreateInfo faceViewInfo{};
        faceViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        faceViewInfo.image = cubeMapImage;
        faceViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        faceViewInfo.format = findDepthFormat();
        faceViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        faceViewInfo.subresourceRange.baseMipLevel = 0;
        faceViewInfo.subresourceRange.levelCount = 1;
        faceViewInfo.subresourceRange.baseArrayLayer = face;
        faceViewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device->getDevice(), &faceViewInfo, nullptr, &faceImageViews[face]) != VK_SUCCESS)
            throw std::runtime_error("failed to create cube shadow face view!");
    }
}

void ShadowCubeMap::createSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    // CLAMP_TO_EDGE for cube maps (avoids seams at face borders)
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;          // manual compare in shader, matches project style
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    if (vkCreateSampler(device->getDevice(), &samplerInfo, nullptr, &cubeMapSampler) != VK_SUCCESS)
        throw std::runtime_error("failed to create cube shadow sampler!");
}

void ShadowCubeMap::createRenderPass()
{
    // Depth-only render pass
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // keep depth for sampling
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;                    // no color output
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                              VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                              VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    if (vkCreateRenderPass(device->getDevice(), &renderPassInfo, nullptr, &cubeMapRenderPass) != VK_SUCCESS)
        throw std::runtime_error("failed to create cube shadow render pass!");
}

void ShadowCubeMap::createFramebuffers()
{
    for (uint32_t face = 0; face < 6; face++) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = cubeMapRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &faceImageViews[face];
        fbInfo.width = cubeMapSize;
        fbInfo.height = cubeMapSize;
        fbInfo.layers = 1;
        if (vkCreateFramebuffer(device->getDevice(), &fbInfo, nullptr, &faceFramebuffers[face]) != VK_SUCCESS)
            throw std::runtime_error("failed to create cube shadow framebuffer!");
    }
}

void ShadowCubeMap::createShaderModules()
{
    // Reuse the existing directional shadow vertex shader (mat4 push constant)
    auto vertCode = EngineUtils::readFile("Shaders/shadow.vert.spv");
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = vertCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(vertCode.data());
    if (vkCreateShaderModule(device->getDevice(), &createInfo, nullptr, &vertShaderModule) != VK_SUCCESS)
        throw std::runtime_error("failed to create cube shadow vert module!");

    // New trivial fragment shader (depth-only pass)
    auto fragCode = EngineUtils::readFile("Shaders/shadowCube.frag.spv");
    createInfo.codeSize = fragCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(fragCode.data());
    if (vkCreateShaderModule(device->getDevice(), &createInfo, nullptr, &fragShaderModule) != VK_SUCCESS)
        throw std::runtime_error("failed to create cube shadow frag module!");
}

void ShadowCubeMap::createPipelineLayout()
{
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT| VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(glm::mat4) + sizeof(glm::vec4);   ;   // per-face viewProj

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 0;        // no descriptor sets needed
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(device->getDevice(), &layoutInfo, nullptr, &cubeMapPipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("failed to create cube shadow pipeline layout!");
}

void ShadowCubeMap::createPipeline()
{
    VkPipelineShaderStageCreateInfo shaderStages[2]{};

    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertShaderModule;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragShaderModule;
    shaderStages[1].pName = "main";

    // Vertex input — reuses the full Vertex layout; shader only reads position
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;   // dynamic
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;    // dynamic

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;              // matches project convention
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_TRUE;
    rasterizer.depthBiasConstantFactor = 1.25f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 1.75f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};

    // Depth-only pass: zero color attachments
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 0;                 // ← key difference vs directional
    colorBlending.pAttachments = nullptr;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = cubeMapPipelineLayout;
    pipelineInfo.renderPass = cubeMapRenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(device->getDevice(), VK_NULL_HANDLE, 1,
                                  &pipelineInfo, nullptr, &cubeMapPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create cube shadow pipeline!");
    }
}

void ShadowCubeMap::cleanup()
{
    auto d = device ? device->getDevice() : VK_NULL_HANDLE;
    if (!d) return;
    if (cubeMapPipeline)        vkDestroyPipeline(d, cubeMapPipeline, nullptr);
    if (cubeMapPipelineLayout)  vkDestroyPipelineLayout(d, cubeMapPipelineLayout, nullptr);
    if (vertShaderModule)       vkDestroyShaderModule(d, vertShaderModule, nullptr);
    if (fragShaderModule)       vkDestroyShaderModule(d, fragShaderModule, nullptr);
    if (cubeMapRenderPass)      vkDestroyRenderPass(d, cubeMapRenderPass, nullptr);
    for (auto& fb : faceFramebuffers) if (fb) vkDestroyFramebuffer(d, fb, nullptr);
    if (cubeMapSampler)         vkDestroySampler(d, cubeMapSampler, nullptr);
    if (cubeMapImageView)       vkDestroyImageView(d, cubeMapImageView, nullptr);
    for (auto& view : faceImageViews) if (view) vkDestroyImageView(d, view, nullptr);
    if (cubeMapImage)           vkDestroyImage(d, cubeMapImage, nullptr);
    if (cubeMapImageMemory)     vkFreeMemory(d, cubeMapImageMemory, nullptr);

    cubeMapImage = VK_NULL_HANDLE;
    cubeMapImageView = VK_NULL_HANDLE;
    cubeMapSampler = VK_NULL_HANDLE;
    cubeMapRenderPass = VK_NULL_HANDLE;
    cubeMapPipeline = VK_NULL_HANDLE;
    cubeMapPipelineLayout = VK_NULL_HANDLE;
    vertShaderModule = VK_NULL_HANDLE;
    fragShaderModule = VK_NULL_HANDLE;
    cubeMapImageMemory = VK_NULL_HANDLE;

    faceImageViews.fill(VK_NULL_HANDLE);
    faceFramebuffers.fill(VK_NULL_HANDLE);
}
