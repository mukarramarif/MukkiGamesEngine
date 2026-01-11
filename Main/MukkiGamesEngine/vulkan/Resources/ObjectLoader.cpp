#include "ObjectLoader.h"
#include "BufferManager.h"
#include "TextureManager.h"
#include <iostream>
#include <stdexcept>
#include <filesystem>

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

bool ObjectLoader::loadGLTF(const std::string& filepath, Model& outModel)
{
	tinygltf::Model gltfModel;
	tinygltf::TinyGLTF loader;
	std::string err, warn;
	
	// Store base path for texture loading
	basePath = std::filesystem::path(filepath).parent_path().string();
	if (!basePath.empty() && basePath.back() != '/' && basePath.back() != '\\') {
		basePath += '/';
	}
	
	bool result = false;
	
	// Check file extension to determine binary or ASCII format
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
	
	// Load textures first (materials reference them)
	loadTextures(gltfModel, outModel);
	
	// Load materials
	loadMaterials(gltfModel, outModel);
	
	// Initialize nodes
	outModel.nodes.resize(gltfModel.nodes.size());
	
	// Process the default scene
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
	model.textures.resize(gltfModel.textures.size());
	
	for (size_t i = 0; i < gltfModel.textures.size(); i++) {
		std::cout << "  Loading texture " << i << "..." << std::endl;
		loadTextureFromGLTF(gltfModel, static_cast<int>(i), model.textures[i]);
	}
}

void ObjectLoader::loadTextureFromGLTF(const tinygltf::Model& gltfModel, int textureIndex, 
                                        LoadedTexture& outTexture)
{
	const tinygltf::Texture& gltfTexture = gltfModel.textures[textureIndex];
	const tinygltf::Image& gltfImage = gltfModel.images[gltfTexture.source];
	
	// Get image data - tinygltf already decodes the image
	const unsigned char* buffer = gltfImage.image.data();
	int width = gltfImage.width;
	int height = gltfImage.height;
	int channels = gltfImage.component;
	
	if (buffer == nullptr || width == 0 || height == 0) {
		std::cerr << "Invalid image data for texture " << textureIndex << std::endl;
		return;
	}
	
	std::cout << "    Image: " << width << "x" << height << ", " << channels << " channels" << std::endl;
	
	// Create the texture
	createTextureFromBuffer(buffer, width, height, channels, outTexture);
	
	// Create sampler based on glTF sampler settings
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	
	if (gltfTexture.sampler >= 0 && gltfTexture.sampler < static_cast<int>(gltfModel.samplers.size())) {
		const tinygltf::Sampler& gltfSampler = gltfModel.samplers[gltfTexture.sampler];
		samplerInfo.magFilter = getVkFilterMode(gltfSampler.magFilter);
		samplerInfo.minFilter = getVkFilterMode(gltfSampler.minFilter);
		samplerInfo.addressModeU = getVkWrapMode(gltfSampler.wrapS);
		samplerInfo.addressModeV = getVkWrapMode(gltfSampler.wrapT);
	} else {
		// Default sampler settings
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

void ObjectLoader::createTextureFromBuffer(const unsigned char* buffer, int width, int height, 
                                            int channels, LoadedTexture& outTexture)
{
	outTexture.width = static_cast<uint32_t>(width);
	outTexture.height = static_cast<uint32_t>(height);
	
	// Always use RGBA format - convert if necessary
	VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;
	std::vector<unsigned char> rgbaBuffer;
	
	const unsigned char* pixelData = buffer;
	
	// Convert to RGBA if not already
	if (channels == 3) {
		rgbaBuffer.resize(width * height * 4);
		for (int i = 0; i < width * height; i++) {
			rgbaBuffer[i * 4 + 0] = buffer[i * 3 + 0];
			rgbaBuffer[i * 4 + 1] = buffer[i * 3 + 1];
			rgbaBuffer[i * 4 + 2] = buffer[i * 3 + 2];
			rgbaBuffer[i * 4 + 3] = 255;
		}
		pixelData = rgbaBuffer.data();
	} else if (channels == 1) {
		// Grayscale to RGBA
		rgbaBuffer.resize(width * height * 4);
		for (int i = 0; i < width * height; i++) {
			rgbaBuffer[i * 4 + 0] = buffer[i];
			rgbaBuffer[i * 4 + 1] = buffer[i];
			rgbaBuffer[i * 4 + 2] = buffer[i];
			rgbaBuffer[i * 4 + 3] = 255;
		}
		pixelData = rgbaBuffer.data();
	}
	
	// Create staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	
	bufferManager->createBuffer(
		imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);
	
	// Copy pixel data to staging buffer
	void* data;
	vkMapMemory(device->getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixelData, static_cast<size_t>(imageSize));
	vkUnmapMemory(device->getDevice(), stagingBufferMemory);
	
	// Create image using TextureManager
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
	
	// Transition and copy
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
	
	// Create image view
	outTexture.imageView = textureManager->createImageView(
		outTexture.image,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_ASPECT_COLOR_BIT
	);
	
	// Cleanup staging buffer
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
	for (const auto& gltfMaterial : gltfModel.materials) {
		Material material;
		material.isTransparent = (gltfMaterial.alphaMode == "BlEND");
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
		
		// Additional textures from additionalValues
		if (gltfMaterial.additionalValues.find("normalTexture") != gltfMaterial.additionalValues.end()) {
			material.normalTextureIndex = gltfMaterial.additionalValues.at("normalTexture").TextureIndex();
		}
		
		model.materials.push_back(material);
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

void ObjectLoader::loadMesh(const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh,
	Model& model, const glm::mat4& worldTransform)
{
	Mesh mesh;
	mesh.name = gltfMesh.name;

	// Calculate normal matrix for transforming normals (for future lighting)
	glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldTransform)));

	for (const auto& primitive : gltfMesh.primitives) {
		Primitive prim;
		prim.firstVertex = static_cast<uint32_t>(model.vertices.size());
		prim.firstIndex = static_cast<uint32_t>(model.indices.size());
		prim.materialIndex = primitive.material;

		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;

		const float* positionBuffer = nullptr;
		const float* normalBuffer = nullptr;
		const float* texCoordBuffer = nullptr;
		const float* colorBuffer = nullptr;
		int colorComponentCount = 3; // Default to RGB

		if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
			const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.attributes.find("POSITION")->second];
			const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
			positionBuffer = reinterpret_cast<const float*>(&gltfModel.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
			vertexCount = static_cast<uint32_t>(accessor.count);
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
			// Check if it's VEC3 or VEC4
			colorComponentCount = (accessor.type == TINYGLTF_TYPE_VEC4) ? 4 : 3;
		}

		// Build vertices with world transform applied
		for (uint32_t v = 0; v < vertexCount; v++) {
			Vertex vertex{};

			// Position - apply world transform
			glm::vec4 localPos = glm::vec4(
				positionBuffer[v * 3 + 0],
				positionBuffer[v * 3 + 1],
				positionBuffer[v * 3 + 2],
				1.0f
			);
			glm::vec4 worldPos = worldTransform * localPos;
			vertex.pos = glm::vec3(worldPos);

			// Vertex color - use actual vertex colors if available, otherwise white
			// White ensures texture is displayed at full brightness
			if (colorBuffer) {
				vertex.color = glm::vec3(
					colorBuffer[v * colorComponentCount + 0],
					colorBuffer[v * colorComponentCount + 1],
					colorBuffer[v * colorComponentCount + 2]
				);
			}
			else {
				// Default to white so textures render correctly
				vertex.color = glm::vec3(1.0f);
			}

			// Texture coordinates
			if (texCoordBuffer) {
				vertex.texCoord = glm::vec2(
					texCoordBuffer[v * 2 + 0],
					texCoordBuffer[v * 2 + 1]
				);
			}
			else {
				vertex.texCoord = glm::vec2(0.0f);
			}

			model.vertices.push_back(vertex);
		}

		// Load indices
		if (primitive.indices > -1) {
			const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.indices];
			const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
			const void* dataPtr = &gltfModel.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset];

			indexCount = static_cast<uint32_t>(accessor.count);

			switch (accessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
				const uint32_t* buf = static_cast<const uint32_t*>(dataPtr);
				for (size_t i = 0; i < accessor.count; i++) {
					model.indices.push_back(buf[i] + prim.firstVertex);
				}
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
				const uint16_t* buf = static_cast<const uint16_t*>(dataPtr);
				for (size_t i = 0; i < accessor.count; i++) {
					model.indices.push_back(buf[i] + prim.firstVertex);
				}
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
				const uint8_t* buf = static_cast<const uint8_t*>(dataPtr);
				for (size_t i = 0; i < accessor.count; i++) {
					model.indices.push_back(buf[i] + prim.firstVertex);
				}
				break;
			}
			default:
				std::cerr << "Unknown index component type!" << std::endl;
				break;
			}
		}

		prim.vertexCount = vertexCount;
		prim.indexCount = indexCount;
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
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, model.vertexBuffer, model.vertexBufferMemory);
	
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
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
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
	model.indices.clear();
	model.meshes.clear();
	model.nodes.clear();
	model.materials.clear();
	model.textures.clear();
}