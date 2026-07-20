#include "ObjectLoader.h"
#include "BufferManager.h"
#include "TextureManager.h"
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <future>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

ObjectLoader::~ObjectLoader()
{
	cleanup();
}

void ObjectLoader::init(Device* device, TextureManager* textureManager, BufferManager* bufferManager)
{
	this->device = device;
	this->textureManager = textureManager;
	this->bufferManager = bufferManager;
}

void ObjectLoader::cleanup()
{
}

std::future<bool> ObjectLoader::loadGLTFAsync(const std::string& filepath, Model& outModel)
{
	return std::async(std::launch::async, [this, filepath, &outModel]() {
		return loadGLTF(filepath, outModel);
	});
}

bool ObjectLoader::loadGLTF(const std::string& filepath, Model& outModel)
{
	tinygltf::Model gltfModel;
	tinygltf::TinyGLTF loader;
	std::string err, warn;

	bool result = false;

	if (filepath.find(".glb") != std::string::npos) {
		result = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filepath);
	} else {
		result = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filepath);
	}

	if (!warn.empty()) {
		std::cout << "glTF Warning: " << warn << std::endl;
	}

	if (!err.empty()) {
		std::cerr << "glTF Error: " << err << std::endl;
		return false;
	}

	if (!result) {
		std::cerr << "Failed to load glTF file: " << filepath << std::endl;
		return false;
	}

	std::cout << "Loading glTF: " << filepath << std::endl;
	std::cout << "  Meshes: " << gltfModel.meshes.size() << std::endl;
	std::cout << "  Materials: " << gltfModel.materials.size() << std::endl;
	std::cout << "  Textures: " << gltfModel.textures.size() << std::endl;
	std::cout << "  Images: " << gltfModel.images.size() << std::endl;
	std::cout << "  Nodes: " << gltfModel.nodes.size() << std::endl;

	loadTextures(gltfModel, outModel);
	loadMaterials(gltfModel, outModel);

	outModel.nodes.resize(gltfModel.nodes.size());

	const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];

	for (int nodeIndex : scene.nodes) {
		outModel.rootNodes.push_back(nodeIndex);
		loadNode(gltfModel, gltfModel.nodes[nodeIndex], nodeIndex, outModel, glm::mat4(1.0f));
	}

	std::cout << "  Loaded vertices: " << outModel.vertices.size() << std::endl;
	std::cout << "  Loaded indices: " << outModel.indices.size() << std::endl;
	std::cout << "  Loaded textures: " << outModel.textures.size() << std::endl;
	for (size_t i = 0; i < outModel.meshes.size(); i++) {
		bool hasTransparent = false;
		for (const auto& prim : outModel.meshes[i].primitives) {
			if (prim.materialIndex >= 0 &&
				prim.materialIndex < static_cast<int32_t>(outModel.materials.size()) &&
				outModel.materials[prim.materialIndex].isTransparent) {
				hasTransparent = true;
				break;
			}
		}

		if (hasTransparent) {
			outModel.transparentMeshIndices.push_back(i);
		}
		else {
			outModel.opaqueMeshIndices.push_back(i);
		}
	}
	return true;
}

void ObjectLoader::loadTextures(const tinygltf::Model& gltfModel, Model& model)
{
	size_t textureCount = gltfModel.textures.size();
	model.textures.resize(textureCount);

	// Phase 1: Convert pixel data in parallel
	std::vector<std::vector<unsigned char>> convertedBuffers(textureCount);
	std::vector<int> widths(textureCount);
	std::vector<int> heights(textureCount);
	std::vector<std::future<void>> conversions;

	for (size_t i = 0; i < textureCount; i++) {
		const tinygltf::Texture& gltfTexture = gltfModel.textures[i];
		const tinygltf::Image& gltfImage = gltfModel.images[gltfTexture.source];

		if (gltfImage.image.empty() || gltfImage.width == 0 || gltfImage.height == 0) {
			continue;
		}

		widths[i] = gltfImage.width;
		heights[i] = gltfImage.height;

		conversions.push_back(std::async(std::launch::async, [&gltfImage, &convertedBuffers, i]() {
			int channels = gltfImage.component;
			int pixelCount = gltfImage.width * gltfImage.height;

			if (channels == 3) {
				convertedBuffers[i].resize(pixelCount * 4);
				for (int j = 0; j < pixelCount; j++) {
					convertedBuffers[i][j * 4 + 0] = gltfImage.image[j * 3 + 0];
					convertedBuffers[i][j * 4 + 1] = gltfImage.image[j * 3 + 1];
					convertedBuffers[i][j * 4 + 2] = gltfImage.image[j * 3 + 2];
					convertedBuffers[i][j * 4 + 3] = 255;
				}
			} else if (channels == 1) {
				convertedBuffers[i].resize(pixelCount * 4);
				for (int j = 0; j < pixelCount; j++) {
					convertedBuffers[i][j * 4 + 0] = gltfImage.image[j];
					convertedBuffers[i][j * 4 + 1] = gltfImage.image[j];
					convertedBuffers[i][j * 4 + 2] = gltfImage.image[j];
					convertedBuffers[i][j * 4 + 3] = 255;
				}
			}
		}));
	}

	for (auto& f : conversions) {
		f.get();
	}

	// Phase 2: Upload textures to GPU sequentially
	for (size_t i = 0; i < textureCount; i++) {
		const tinygltf::Texture& gltfTexture = gltfModel.textures[i];
		const tinygltf::Image& gltfImage = gltfModel.images[gltfTexture.source];

		if (gltfImage.image.empty() || gltfImage.width == 0 || gltfImage.height == 0) {
			std::cerr << "Invalid image data for texture " << i << std::endl;
			continue;
		}

		int channels = gltfImage.component;
		LoadedTexture& outTexture = model.textures[i];
		outTexture.width = static_cast<uint32_t>(widths[i]);
		outTexture.height = static_cast<uint32_t>(heights[i]);

		const unsigned char* pixelData = channels == 4
			? gltfImage.image.data()
			: convertedBuffers[i].data();

		uploadTextureToGPU(pixelData, widths[i], heights[i], outTexture);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

		if (gltfTexture.sampler >= 0 && gltfTexture.sampler < static_cast<int>(gltfModel.samplers.size())) {
			const tinygltf::Sampler& gltfSampler = gltfModel.samplers[gltfTexture.sampler];
			samplerInfo.magFilter = getVkFilterMode(gltfSampler.magFilter);
			samplerInfo.minFilter = getVkFilterMode(gltfSampler.minFilter);
			samplerInfo.addressModeU = getVkWrapMode(gltfSampler.wrapS);
			samplerInfo.addressModeV = getVkWrapMode(gltfSampler.wrapT);
		} else {
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}

		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;

		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(device->getPhysicalDevice(), &properties);
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		if (vkCreateSampler(device->getDevice(), &samplerInfo, nullptr, &outTexture.sampler) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture sampler!");
		}
	}
}

void ObjectLoader::uploadTextureToGPU(const unsigned char* pixelData, int width, int height,
                                      LoadedTexture& outTexture)
{
	VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	bufferManager->createBuffer(
		imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	void* data;
	vkMapMemory(device->getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixelData, static_cast<size_t>(imageSize));
	vkUnmapMemory(device->getDevice(), stagingBufferMemory);

	textureManager->createImage(
		static_cast<uint32_t>(width),
		static_cast<uint32_t>(height),
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		outTexture.image,
		outTexture.memory
	);

	textureManager->transitionImageLayout(
		outTexture.image,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

	textureManager->copyBufferToImage(
		stagingBuffer,
		outTexture.image,
		static_cast<uint32_t>(width),
		static_cast<uint32_t>(height)
	);

	textureManager->transitionImageLayout(
		outTexture.image,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	outTexture.imageView = textureManager->createImageView(
		outTexture.image,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	bufferManager->destroyBuffer(stagingBuffer, stagingBufferMemory);
}

VkSamplerAddressMode ObjectLoader::getVkWrapMode(int wrapMode)
{
	switch (wrapMode) {
		case TINYGLTF_TEXTURE_WRAP_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		default:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
}

VkFilter ObjectLoader::getVkFilterMode(int filterMode)
{
	switch (filterMode) {
		case TINYGLTF_TEXTURE_FILTER_NEAREST:
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
			return VK_FILTER_NEAREST;
		case TINYGLTF_TEXTURE_FILTER_LINEAR:
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
		default:
			return VK_FILTER_LINEAR;
	}
}

void ObjectLoader::loadMaterials(const tinygltf::Model& gltfModel, Model& model)
{
	model.materials.reserve(gltfModel.materials.size());
	for (const auto& gltfMaterial : gltfModel.materials) {
		Material material;
		material.isTransparent = (gltfMaterial.alphaMode == "BLEND");

		if(gltfMaterial.alphaMode == "MASK") {
			material.alphaCutoff = static_cast<float>(gltfMaterial.alphaCutoff);
		}
		// PBR Metallic Roughness workflow
		if (gltfMaterial.values.find("baseColorFactor") != gltfMaterial.values.end()) {
			material.baseColorFactor = glm::make_vec4(gltfMaterial.values.at("baseColorFactor").ColorFactor().data());
		}
		if (gltfMaterial.values.find("metallicFactor") != gltfMaterial.values.end()) {
			material.metallicFactor = static_cast<float>(gltfMaterial.values.at("metallicFactor").Factor());
		}
		if (gltfMaterial.values.find("roughnessFactor") != gltfMaterial.values.end()) {
			material.roughnessFactor = static_cast<float>(gltfMaterial.values.at("roughnessFactor").Factor());
		}
		if (gltfMaterial.values.find("baseColorTexture") != gltfMaterial.values.end()) {
			material.baseColorTextureIndex = gltfMaterial.values.at("baseColorTexture").TextureIndex();
		}

		model.materials.push_back(material);
	}
	// Process emissive properties
	for (size_t i = 0; i < gltfModel.materials.size(); i++) {
		const auto& gltfMat = gltfModel.materials[i];
		Material& mat = model.materials[i];
		if (!gltfMat.emissiveFactor.empty()) {
			mat.emissiveFactor = glm::vec3(
				gltfMat.emissiveFactor[0],
				gltfMat.emissiveFactor[1],
				gltfMat.emissiveFactor[2]
			);
		}

		if (gltfMat.emissiveTexture.index >= 0) {
			mat.emissiveTextureIndex = gltfMat.emissiveTexture.index;
		}

		// Mark as emissive if it has emissive texture or non-zero emissive factor
		// Also check material name for "lightflare" or similar
		mat.isEmissive = (mat.emissiveTextureIndex >= 0) ||
			(glm::length(mat.emissiveFactor) > 0.01f) ||
			(gltfMat.name.find("lightflare") != std::string::npos) ||
			(gltfMat.name.find("flare") != std::string::npos) ||
			(gltfMat.name.find("glow") != std::string::npos) ||
			(gltfMat.name.find("emissive") != std::string::npos);
	}
	// Add default material if none exist
	if (model.materials.empty()) {
		model.materials.push_back(Material{});
	}
}

void ObjectLoader::loadNode(const tinygltf::Model& gltfModel, const tinygltf::Node& gltfNode,
                            int nodeIndex, Model& model, const glm::mat4& parentTransform)
{
	Node& node = model.nodes[nodeIndex];
	node.name = gltfNode.name;
	node.localTransform = getNodeTransform(gltfNode);
	node.worldTransform = parentTransform * node.localTransform;

	// Load mesh if present - pass the world transform to apply to vertices
	if (gltfNode.mesh > -1) {
		node.meshIndex = static_cast<int32_t>(model.meshes.size());
		loadMesh(gltfModel, gltfModel.meshes[gltfNode.mesh], model, node.worldTransform);
	}

	// Process children
	for (int childIndex : gltfNode.children) {
		node.children.push_back(childIndex);
		model.nodes[childIndex].parent = nodeIndex;
		loadNode(gltfModel, gltfModel.nodes[childIndex], childIndex, model, node.worldTransform);
	}
}

ObjectLoader::PrimitiveData ObjectLoader::loadPrimitiveData(const tinygltf::Model& gltfModel,
                                                            const tinygltf::Primitive& primitive,
                                                            const glm::mat4& worldTransform,
                                                            const glm::mat3& normalMatrix)
{
	PrimitiveData result;

	const float* positionBuffer = nullptr;
	const float* normalBuffer = nullptr;
	const float* texCoordBuffer = nullptr;
	const float* colorBuffer = nullptr;
	int colorComponentCount = 3;

	if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
		const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.attributes.find("POSITION")->second];
		const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
		positionBuffer = reinterpret_cast<const float*>(&gltfModel.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
		result.vertexCount = static_cast<uint32_t>(accessor.count);
	}

	if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
		const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.attributes.find("NORMAL")->second];
		const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
		normalBuffer = reinterpret_cast<const float*>(&gltfModel.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
	}

	if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
		const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.attributes.find("TEXCOORD_0")->second];
		const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
		texCoordBuffer = reinterpret_cast<const float*>(&gltfModel.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
	}

	if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
		const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.attributes.find("COLOR_0")->second];
		const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
		colorBuffer = reinterpret_cast<const float*>(&gltfModel.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
		colorComponentCount = (accessor.type == TINYGLTF_TYPE_VEC4) ? 4 : 3;
	}

	result.vertices.reserve(result.vertexCount);
	result.rtVertices.reserve(result.vertexCount);

	for (uint32_t v = 0; v < result.vertexCount; v++) {
		Vertex vertex{};
		RayTracingVertex rtVertex{};

		glm::vec3 localPos = glm::vec3(
			positionBuffer[v * 3 + 0],
			positionBuffer[v * 3 + 1],
			positionBuffer[v * 3 + 2]
		);
		glm::vec4 worldPos = worldTransform * glm::vec4(localPos, 1.0f);
		vertex.pos = glm::vec3(worldPos);
		rtVertex.position = glm::vec4(localPos, 1.0f);

		if (colorBuffer) {
			vertex.color = glm::vec3(
				colorBuffer[v * colorComponentCount + 0],
				colorBuffer[v * colorComponentCount + 1],
				colorBuffer[v * colorComponentCount + 2]
			);
		} else {
			vertex.color = glm::vec3(1.0f);
		}

		if (texCoordBuffer) {
			vertex.texCoord = glm::vec2(
				texCoordBuffer[v * 2 + 0],
				texCoordBuffer[v * 2 + 1]
			);
		} else {
			vertex.texCoord = glm::vec2(0.0f);
		}
		rtVertex.texCoord = vertex.texCoord;

		glm::vec3 localNormal = glm::vec3(0.0f, 0.0f, 1.0f);
		if (normalBuffer) {
			localNormal = glm::vec3(
				normalBuffer[v * 3 + 0],
				normalBuffer[v * 3 + 1],
				normalBuffer[v * 3 + 2]
			);
		}

		vertex.normal = normalMatrix * localNormal;
		rtVertex.normal = glm::vec4(glm::normalize(localNormal), 0.0f);

		result.vertices.push_back(vertex);
		result.rtVertices.push_back(rtVertex);
	}

	if (primitive.indices > -1) {
		const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.indices];
		const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
		const void* dataPtr = &gltfModel.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset];

		result.indexCount = static_cast<uint32_t>(accessor.count);
		result.indices.reserve(result.indexCount);

		switch (accessor.componentType) {
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
			const uint32_t* buf = static_cast<const uint32_t*>(dataPtr);
			for (size_t i = 0; i < accessor.count; i++) {
				result.indices.push_back(buf[i]);
			}
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
			const uint16_t* buf = static_cast<const uint16_t*>(dataPtr);
			for (size_t i = 0; i < accessor.count; i++) {
				result.indices.push_back(buf[i]);
			}
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
			const uint8_t* buf = static_cast<const uint8_t*>(dataPtr);
			for (size_t i = 0; i < accessor.count; i++) {
				result.indices.push_back(buf[i]);
			}
			break;
		}
		default:
			std::cerr << "Unknown index component type!" << std::endl;
			break;
		}
	}

	return result;
}

void ObjectLoader::loadMesh(const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh,
	Model& model, const glm::mat4& worldTransform)
{
	Mesh mesh;
	mesh.name = gltfMesh.name;

	glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldTransform)));

	// Process primitives in parallel
	size_t primCount = gltfMesh.primitives.size();
	std::vector<std::future<PrimitiveData>> futures;
	futures.reserve(primCount);

	for (const auto& primitive : gltfMesh.primitives) {
		futures.push_back(std::async(std::launch::async, &ObjectLoader::loadPrimitiveData, this,
		                             std::ref(gltfModel), std::ref(primitive),
		                             std::ref(worldTransform), std::ref(normalMatrix)));
	}

	uint32_t vertexOffset = static_cast<uint32_t>(model.vertices.size());
	uint32_t indexOffset = static_cast<uint32_t>(model.indices.size());

	for (size_t pi = 0; pi < primCount; pi++) {
		PrimitiveData data = futures[pi].get();

		Primitive prim;
		prim.firstVertex = vertexOffset;
		prim.firstIndex = indexOffset;
		prim.materialIndex = gltfMesh.primitives[pi].material;
		prim.vertexCount = data.vertexCount;
		prim.indexCount = data.indexCount;

		// Merge collected data into model
		model.vertices.insert(model.vertices.end(), data.vertices.begin(), data.vertices.end());
		model.rtVertices.insert(model.rtVertices.end(), data.rtVertices.begin(), data.rtVertices.end());

		for (uint32_t idx : data.indices) {
			model.indices.push_back(idx + vertexOffset);
		}

		vertexOffset += data.vertexCount;
		indexOffset += data.indexCount;

		mesh.primitives.push_back(prim);
	}

	model.meshes.push_back(mesh);
}

glm::mat4 ObjectLoader::getNodeTransform(const tinygltf::Node& node)
{
	glm::mat4 transform = glm::mat4(1.0f);

	if (node.matrix.size() == 16) {
		// Use matrix directly
		transform = glm::make_mat4(node.matrix.data());
	} else {
		// Build from TRS
		if (node.translation.size() == 3) {
			transform = glm::translate(transform, glm::vec3(
				node.translation[0], node.translation[1], node.translation[2]
			));
		}
		if (node.rotation.size() == 4) {
			glm::quat q(
				static_cast<float>(node.rotation[3]),
				static_cast<float>(node.rotation[0]),
				static_cast<float>(node.rotation[1]),
				static_cast<float>(node.rotation[2])
			);
			transform *= glm::mat4_cast(q);
		}
		if (node.scale.size() == 3) {
			transform = glm::scale(transform, glm::vec3(
				node.scale[0], node.scale[1], node.scale[2]
			));
		}
	}

	return transform;
}

void ObjectLoader::createModelBuffers(Model& model)
{
	if (model.vertices.empty() || model.indices.empty()) {
		std::cerr << "Cannot create buffers for empty model!" << std::endl;
		return;
	}

	// Vertex buffer
	VkDeviceSize vertexBufferSize = sizeof(Vertex) * model.vertices.size();
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	device->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(device->getDevice(), stagingBufferMemory, 0, vertexBufferSize, 0, &data);
	memcpy(data, model.vertices.data(), vertexBufferSize);
	vkUnmapMemory(device->getDevice(), stagingBufferMemory);

	device->createBuffer(vertexBufferSize,
       VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, model.vertexBuffer, model.vertexBufferMemory);

	// Ray tracing vertex buffer (local positions/normals)
	VkDeviceSize rtVertexBufferSize = sizeof(RayTracingVertex) * model.rtVertices.size();
	if (rtVertexBufferSize > 0) {
		VkBuffer rtStagingBuffer = VK_NULL_HANDLE;
		VkDeviceMemory rtStagingMemory = VK_NULL_HANDLE;
		device->createBuffer(rtVertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			rtStagingBuffer, rtStagingMemory);

		vkMapMemory(device->getDevice(), rtStagingMemory, 0, rtVertexBufferSize, 0, &data);
		memcpy(data, model.rtVertices.data(), rtVertexBufferSize);
		vkUnmapMemory(device->getDevice(), rtStagingMemory);

		device->createBuffer(rtVertexBufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, model.rtVertexBuffer, model.rtVertexBufferMemory);

		device->copyBuffer(rtStagingBuffer, model.rtVertexBuffer, rtVertexBufferSize);
		vkDestroyBuffer(device->getDevice(), rtStagingBuffer, nullptr);
		vkFreeMemory(device->getDevice(), rtStagingMemory, nullptr);
	}

	device->copyBuffer(stagingBuffer, model.vertexBuffer, vertexBufferSize);
	vkDestroyBuffer(device->getDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device->getDevice(), stagingBufferMemory, nullptr);

	// Index buffer
	VkDeviceSize indexBufferSize = sizeof(uint32_t) * model.indices.size();

	device->createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	vkMapMemory(device->getDevice(), stagingBufferMemory, 0, indexBufferSize, 0, &data);
	memcpy(data, model.indices.data(), indexBufferSize);
	vkUnmapMemory(device->getDevice(), stagingBufferMemory);

	device->createBuffer(indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, model.indexBuffer, model.indexBufferMemory);

	device->copyBuffer(stagingBuffer, model.indexBuffer, indexBufferSize);
	vkDestroyBuffer(device->getDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device->getDevice(), stagingBufferMemory, nullptr);

	std::cout << "Created GPU buffers - Vertices: " << vertexBufferSize
	          << " bytes, Indices: " << indexBufferSize << " bytes" << std::endl;
}

void ObjectLoader::destroyModel(Model& model)
{
	// Destroy buffers
	if (model.vertexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device->getDevice(), model.vertexBuffer, nullptr);
	}
	if (model.vertexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device->getDevice(), model.vertexBufferMemory, nullptr);
	}
	if (model.indexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device->getDevice(), model.indexBuffer, nullptr);
	}
	if (model.indexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device->getDevice(), model.indexBufferMemory, nullptr);
	}

	// Destroy ray tracing vertex buffer
	if (model.rtVertexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device->getDevice(), model.rtVertexBuffer, nullptr);
		model.rtVertexBuffer = VK_NULL_HANDLE;
	}
	if (model.rtVertexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device->getDevice(), model.rtVertexBufferMemory, nullptr);
		model.rtVertexBufferMemory = VK_NULL_HANDLE;
	}

	// Destroy textures
	for (auto& texture : model.textures) {
		if (texture.sampler != VK_NULL_HANDLE) {
			vkDestroySampler(device->getDevice(), texture.sampler, nullptr);
		}
		if (texture.imageView != VK_NULL_HANDLE) {
			vkDestroyImageView(device->getDevice(), texture.imageView, nullptr);
		}
		if (texture.image != VK_NULL_HANDLE) {
			vkDestroyImage(device->getDevice(), texture.image, nullptr);
		}
		if (texture.memory != VK_NULL_HANDLE) {
			vkFreeMemory(device->getDevice(), texture.memory, nullptr);
		}
	}

	model.vertices.clear();
	model.rtVertices.clear();
	model.indices.clear();
	model.meshes.clear();
	model.nodes.clear();
	model.materials.clear();
	model.textures.clear();
}
