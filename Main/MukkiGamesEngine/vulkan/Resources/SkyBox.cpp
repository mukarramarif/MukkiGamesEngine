#include "SkyBox.h"
#include <array>
#include <stdexcept>



SkyBox::SkyBox() {}

SkyBox::~SkyBox() {}

void SkyBox::init(Device* device, TextureManager* textureManager, BufferManager* bufferManager,
	VkRenderPass renderPass, const std::string& cubemapPath,
	CubemapLayout layout, uint32_t maxFramesInFlight)
{
	this->device = device;
	this->textureManager = textureManager;
	this->bufferManager = bufferManager;
	this->maxFramesInFlight = maxFramesInFlight;

	// Load cubemap texture
	textureManager->createCubemapImage(cubemapPath, cubemapImage, cubemapImageMemory, layout);
	cubemapImageView = textureManager->createImageView(cubemapImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, true);
	textureManager->createTextureSampler(cubemapSampler, true);

	createUniformBuffers();
	createDescriptorSetLayout();
	createDescriptorPool();
	createDescriptorSets();
	createPipelineLayout();
	createPipeline(renderPass);
}

void SkyBox::createUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(SkyboxUBO);

	uniformBuffers.resize(maxFramesInFlight);
	uniformBuffersMemory.resize(maxFramesInFlight);
	uniformBuffersMapped.resize(maxFramesInFlight);

	for (size_t i = 0; i < maxFramesInFlight; i++) {
		bufferManager->createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			uniformBuffers[i], uniformBuffersMemory[i]);

		vkMapMemory(device->getDevice(), uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
	}
}

void SkyBox::createDescriptorSetLayout()
{
	// Binding 1: Cubemap sampler
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	// Binding 2: Uniform buffer (VP matrix)
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 2;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { samplerLayoutBinding, uboLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(device->getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create skybox descriptor set layout!");
	}
}

void SkyBox::createPipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(device->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create skybox pipeline layout!");
	}
}

void SkyBox::createPipeline(VkRenderPass renderPass)
{
	PipelineConfigInfo configInfo{};
	VulkanPipeline::defaultPipelineConfigInfo(configInfo);

	// Skybox-specific settings:
	// 1. Depth test enabled but depth write disabled (skybox always at max depth)
	configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
	configInfo.depthStencilInfo.depthWriteEnable = VK_FALSE;
	configInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	// 2. No culling needed - vertices are in shader with correct winding
	configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

	// 3. NO vertex input - vertices are generated in the shader
	configInfo.bindingDescriptions.clear();
	configInfo.attributeDescriptions.clear();

	configInfo.pipelineLayout = pipelineLayout;
	configInfo.renderPass = renderPass;

	skyboxPipeline = new VulkanPipeline(device, "skybox.vert.spv", "skybox.frag.spv", configInfo);
}

void SkyBox::createDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(maxFramesInFlight);
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(maxFramesInFlight);

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<uint32_t>(maxFramesInFlight);

	if (vkCreateDescriptorPool(device->getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create skybox descriptor pool!");
	}
}

void SkyBox::createDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(maxFramesInFlight);
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(maxFramesInFlight);
	if (vkAllocateDescriptorSets(device->getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate skybox descriptor sets!");
	}

	for (size_t i = 0; i < maxFramesInFlight; i++) {
		// Cubemap sampler - binding 1
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = cubemapImageView;
		imageInfo.sampler = cubemapSampler;

		// Uniform buffer - binding 2
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(SkyboxUBO);

		std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSets[i];
		descriptorWrites[0].dstBinding = 1;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pImageInfo = &imageInfo;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSets[i];
		descriptorWrites[1].dstBinding = 2;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(device->getDevice(), static_cast<uint32_t>(descriptorWrites.size()),
			descriptorWrites.data(), 0, nullptr);
	}
}

void SkyBox::updateUniformBuffer(uint32_t currentFrame, const glm::mat4& view)
{
	SkyboxUBO ubo{};
	
	ubo.VP =  view;

	memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}

void SkyBox::s_recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t currentFrame)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline->getGraphicsPipeline());

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
		0, 1, &descriptorSets[currentFrame], 0, nullptr);

	// Draw 36 vertices (12 triangles for cube) - no vertex buffer needed
	vkCmdDraw(commandBuffer, 36, 1, 0, 0);
}

void SkyBox::cleanup()
{
	if (device == nullptr) return;

	for (size_t i = 0; i < maxFramesInFlight; i++) {
		vkDestroyBuffer(device->getDevice(), uniformBuffers[i], nullptr);
		vkFreeMemory(device->getDevice(), uniformBuffersMemory[i], nullptr);
	}

	// No vertex buffer to destroy anymore

	textureManager->destroySampler(cubemapSampler);
	textureManager->destroyImageView(cubemapImageView);
	textureManager->destroyImage(cubemapImage, cubemapImageMemory);

	vkDestroyDescriptorPool(device->getDevice(), descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device->getDevice(), descriptorSetLayout, nullptr);
	vkDestroyPipelineLayout(device->getDevice(), pipelineLayout, nullptr);

	if (skyboxPipeline) {
		delete skyboxPipeline;
		skyboxPipeline = nullptr;
	}

	device = nullptr;
}