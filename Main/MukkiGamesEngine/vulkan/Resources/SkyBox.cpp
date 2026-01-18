#include "SkyBox.h"
#include <array>
#include <stdexcept>

// Skybox cube vertices (positions only)
static const std::vector<glm::vec3> skyboxVertices = {
	// Back face
	{-1.0f,  1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f},
	{ 1.0f, -1.0f, -1.0f}, { 1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
	// Front face
	{-1.0f, -1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f}, { 1.0f,  1.0f,  1.0f},
	{ 1.0f,  1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f}, {-1.0f, -1.0f,  1.0f},
	// Left face
	{-1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f},
	{-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f},
	// Right face
	{ 1.0f,  1.0f, -1.0f}, { 1.0f,  1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f},
	{ 1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f, -1.0f}, { 1.0f,  1.0f, -1.0f},
	// Top face
	{-1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f,  1.0f}, { 1.0f,  1.0f,  1.0f},
	{ 1.0f,  1.0f,  1.0f}, { 1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
	// Bottom face
	{-1.0f, -1.0f,  1.0f}, {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f},
	{ 1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f,  1.0f}, {-1.0f, -1.0f,  1.0f}
};

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

	createVertexBuffer();
	createUniformBuffers();
	createDescriptorSetLayout();
	createDescriptorPool();
	createDescriptorSets();
	createPipelineLayout();
	createPipeline(renderPass);
}

void SkyBox::createVertexBuffer()
{
	VkDeviceSize bufferSize = sizeof(glm::vec3) * skyboxVertices.size();
	vertexCount = static_cast<uint32_t>(skyboxVertices.size());

	// Create staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	bufferManager->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(device->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, skyboxVertices.data(), bufferSize);
	vkUnmapMemory(device->getDevice(), stagingBufferMemory);

	// Create device local buffer
	bufferManager->createBuffer(bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBuffer, vertexBufferMemory);

	bufferManager->copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
	bufferManager->destroyBuffer(stagingBuffer, stagingBufferMemory);
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
	std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

	// UBO binding
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	bindings[0].pImmutableSamplers = nullptr;

	// Cubemap sampler binding
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].pImmutableSamplers = nullptr;

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

	// 2. Cull front faces (we're inside the cube looking out)
	configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_FRONT_BIT;

	// 3. Skybox vertex input (only position, no normals/texcoords)
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(glm::vec3);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attributeDescription{};
	attributeDescription.binding = 0;
	attributeDescription.location = 0;
	attributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescription.offset = 0;

	configInfo.bindingDescriptions = { bindingDescription };
	configInfo.attributeDescriptions = { attributeDescription };

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
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(SkyboxUBO);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = cubemapImageView;
		imageInfo.sampler = cubemapSampler;

		std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(device->getDevice(), static_cast<uint32_t>(descriptorWrites.size()),
			descriptorWrites.data(), 0, nullptr);
	}
}

void SkyBox::updateUniformBuffer(uint32_t currentFrame, const glm::mat4& view, const glm::mat4& projection)
{
	SkyboxUBO ubo{};
	// Remove translation from view matrix - skybox appears infinitely far
	ubo.view = glm::mat4(glm::mat3(view));
	ubo.projection = projection;

	memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}

void SkyBox::s_recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t currentFrame)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline->getGraphicsPipeline());

	VkBuffer vertexBuffers[] = { vertexBuffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
		0, 1, &descriptorSets[currentFrame], 0, nullptr);

	vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
}

void SkyBox::cleanup()
{
	if (device == nullptr) return;

	for (size_t i = 0; i < maxFramesInFlight; i++) {
		vkDestroyBuffer(device->getDevice(), uniformBuffers[i], nullptr);
		vkFreeMemory(device->getDevice(), uniformBuffersMemory[i], nullptr);
	}

	vkDestroyBuffer(device->getDevice(), vertexBuffer, nullptr);
	vkFreeMemory(device->getDevice(), vertexBufferMemory, nullptr);

	textureManager->destroySampler(cubemapSampler);
	textureManager->destroyImageView(cubemapImageView);
	textureManager->destroyImage(cubemapImage, cubemapImageMemory);

	if (skyboxPipeline) {
		delete skyboxPipeline;
		skyboxPipeline = nullptr;
	}

	vkDestroyPipelineLayout(device->getDevice(), pipelineLayout, nullptr);
	vkDestroyDescriptorPool(device->getDevice(), descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device->getDevice(), descriptorSetLayout, nullptr);
}