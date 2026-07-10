#include "ShadowMap.h"
#include "../utils/utils.h"
#include "../objects/vertex.h"
#include <iostream>
#include <stdexcept>
#include <array>

ShadowMap::~ShadowMap()
{
	cleanup();
}

void ShadowMap::init(Device* device, uint32_t shadowMapSize)
{
	this->device = device;
	this->shadowMapSize = shadowMapSize;

	createShadowImage();
	createDepthImage();
	createShadowRenderPass();
	createShadowSampler();
	createShadowFramebuffer();
	createShaderModules();
	createPipelineLayout();
	createPipeline();
}

void ShadowMap::cleanup()
{
	if (device == nullptr) return;
	VkDevice vkDev = device->getDevice();

	if (shadowPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(vkDev, shadowPipeline, nullptr);
		shadowPipeline = VK_NULL_HANDLE;
	}
	if (shadowPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(vkDev, shadowPipelineLayout, nullptr);
		shadowPipelineLayout = VK_NULL_HANDLE;
	}
	if (vertShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(vkDev, vertShaderModule, nullptr);
		vertShaderModule = VK_NULL_HANDLE;
	}
	if (fragShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(vkDev, fragShaderModule, nullptr);
		fragShaderModule = VK_NULL_HANDLE;
	}

	if (shadowFramebuffer != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(vkDev, shadowFramebuffer, nullptr);
		shadowFramebuffer = VK_NULL_HANDLE;
	}
	if (shadowSampler != VK_NULL_HANDLE) {
		vkDestroySampler(vkDev, shadowSampler, nullptr);
		shadowSampler = VK_NULL_HANDLE;
	}
	if (shadowMapImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(vkDev, shadowMapImageView, nullptr);
		shadowMapImageView = VK_NULL_HANDLE;
	}
	if (shadowMapDepthImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(vkDev, shadowMapDepthImageView, nullptr);
		shadowMapDepthImageView = VK_NULL_HANDLE;
	}
	if (shadowMapDepthImage != VK_NULL_HANDLE) {
		vkDestroyImage(vkDev, shadowMapDepthImage, nullptr);
		shadowMapDepthImage = VK_NULL_HANDLE;
	}
	if (shadowMapDepthMemory != VK_NULL_HANDLE) {
		vkFreeMemory(vkDev, shadowMapDepthMemory, nullptr);
		shadowMapDepthMemory = VK_NULL_HANDLE;
	}
	if (shadowMapImage != VK_NULL_HANDLE) {
		vkDestroyImage(vkDev, shadowMapImage, nullptr);
		shadowMapImage = VK_NULL_HANDLE;
	}
	if (shadowMapImageMemory != VK_NULL_HANDLE) {
		vkFreeMemory(vkDev, shadowMapImageMemory, nullptr);
		shadowMapImageMemory = VK_NULL_HANDLE;
	}
}

VkFormat ShadowMap::findDepthFormat()
{
	return findSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

VkFormat ShadowMap::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	VkPhysicalDevice physicalDevice = device->getPhysicalDevice();
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}
	throw std::runtime_error("failed to find supported format!");
}

void ShadowMap::createShadowImage()
{
	// Shadow map uses R32_SFLOAT (color format) so it can be sampled
	// as a regular float texture without any depth-format SPIR-V issues
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = shadowMapSize;
	imageInfo.extent.height = shadowMapSize;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = VK_FORMAT_R32_SFLOAT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.flags = 0;

	VkDevice vkDev = device->getDevice();

	if (vkCreateImage(vkDev, &imageInfo, nullptr, &shadowMapImage) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow map image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(vkDev, shadowMapImage, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(vkDev, &allocInfo, nullptr, &shadowMapImageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate shadow map image memory!");
	}

	vkBindImageMemory(vkDev, shadowMapImage, shadowMapImageMemory, 0);

	// Create color image view for the shadow map
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = shadowMapImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R32_SFLOAT;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(vkDev, &viewInfo, nullptr, &shadowMapImageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow map image view!");
	}

	std::cout << "Created shadow map: " << shadowMapSize << "x" << shadowMapSize << std::endl;
}

void ShadowMap::createDepthImage()
{
	// Temporary depth buffer for the shadow pass (depth testing)
	VkFormat depthFormat = findDepthFormat();

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = shadowMapSize;
	imageInfo.extent.height = shadowMapSize;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = depthFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.flags = 0;

	VkDevice vkDev = device->getDevice();

	if (vkCreateImage(vkDev, &imageInfo, nullptr, &shadowMapDepthImage) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow map depth image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(vkDev, shadowMapDepthImage, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(vkDev, &allocInfo, nullptr, &shadowMapDepthMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate shadow map depth memory!");
	}

	vkBindImageMemory(vkDev, shadowMapDepthImage, shadowMapDepthMemory, 0);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = shadowMapDepthImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = depthFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(vkDev, &viewInfo, nullptr, &shadowMapDepthImageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow map depth image view!");
	}
}

void ShadowMap::createShadowRenderPass()
{
	// Color attachment: R32_SFLOAT shadow map (stored for sampling)
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = VK_FORMAT_R32_SFLOAT;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth attachment: temporary depth buffer (not stored)
	VkFormat depthFormat = findDepthFormat();
	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency.dependencyFlags = 0;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device->getDevice(), &renderPassInfo, nullptr, &shadowRenderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow render pass!");
	}
}

void ShadowMap::createShadowSampler()
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	if (vkCreateSampler(device->getDevice(), &samplerInfo, nullptr, &shadowSampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow sampler!");
	}
}

void ShadowMap::createShadowFramebuffer()
{
	std::array<VkImageView, 2> attachments = { shadowMapImageView, shadowMapDepthImageView };

	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = shadowRenderPass;
	framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebufferInfo.pAttachments = attachments.data();
	framebufferInfo.width = shadowMapSize;
	framebufferInfo.height = shadowMapSize;
	framebufferInfo.layers = 1;

	if (vkCreateFramebuffer(device->getDevice(), &framebufferInfo, nullptr, &shadowFramebuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow framebuffer!");
	}
}

void ShadowMap::createShaderModules()
{
	auto vertShaderCode = EngineUtils::readFile("Shaders/shadow.vert.spv");
	auto fragShaderCode = EngineUtils::readFile("Shaders/shadow.frag.spv");

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = vertShaderCode.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());

	if (vkCreateShaderModule(device->getDevice(), &createInfo, nullptr, &vertShaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow vertex shader module!");
	}

	createInfo.codeSize = fragShaderCode.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());

	if (vkCreateShaderModule(device->getDevice(), &createInfo, nullptr, &fragShaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow fragment shader module!");
	}
}

void ShadowMap::createPipelineLayout()
{
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(glm::mat4);

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 0;
	layoutInfo.pSetLayouts = nullptr;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(device->getDevice(), &layoutInfo, nullptr, &shadowPipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow pipeline layout!");
	}
}

void ShadowMap::createPipeline()
{
	VkPipelineShaderStageCreateInfo shaderStages[2]{};

	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].module = vertShaderModule;
	shaderStages[0].pName = "main";
	shaderStages[0].flags = 0;
	shaderStages[0].pNext = nullptr;
	shaderStages[0].pSpecializationInfo = nullptr;

	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].module = fragShaderModule;
	shaderStages[1].pName = "main";
	shaderStages[1].flags = 0;
	shaderStages[1].pNext = nullptr;
	shaderStages[1].pSpecializationInfo = nullptr;

	// Vertex input - reuse Vertex struct (only position is used by shader)
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
	viewportState.pViewports = nullptr;
	viewportState.scissorCount = 1;
	viewportState.pScissors = nullptr;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
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

	// Color blend: write to R channel of R32_SFLOAT shadow map
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
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
	dynamicState.flags = 0;

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
	pipelineInfo.layout = shadowPipelineLayout;
	pipelineInfo.renderPass = shadowRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(device->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &shadowPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow pipeline!");
	}
}
