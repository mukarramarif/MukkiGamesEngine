#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tiny_gltf.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "../Core/VkDevice.h"
#include "../objects/vertex.h"

class TextureManager;
class BufferManager;
// Material data for PBR rendering
struct Material {
	glm::vec4 baseColorFactor = glm::vec4(1.0f);
	float metallicFactor = 1.0f;
	float roughnessFactor = 1.0f;
	int32_t baseColorTextureIndex = -1;
	int32_t normalTextureIndex = -1;
	int32_t metallicRoughnessTextureIndex = -1;
	int32_t emissiveTextureIndex = -1;
	glm::vec3 emissiveFactor = glm::vec3(0.0f);
	bool isTransparent = false;
	bool isEmissive = false;
	float alphaCutoff = 0.5f;
};

// A single mesh primitive (submesh)
struct Primitive {
	uint32_t firstIndex;
	uint32_t indexCount;
	uint32_t firstVertex;
	uint32_t vertexCount;
	int32_t materialIndex = -1;
};

// A mesh can contain multiple primitives
struct Mesh {
	std::string name;
	std::vector<Primitive> primitives;
};

// A node in the scene hierarchy
struct Node {
	std::string name;
	glm::mat4 localTransform = glm::mat4(1.0f);
	glm::mat4 worldTransform = glm::mat4(1.0f);
	int32_t meshIndex = -1;
	std::vector<int32_t> children;
	int32_t parent = -1;
};

// Texture loaded from glTF
struct LoadedTexture {
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	uint32_t width = 0;
	uint32_t height = 0;
};

// Complete loaded model
struct Model {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<Mesh> meshes;
	std::vector<Node> nodes;
	std::vector<Material> materials;
	std::vector<LoadedTexture> textures;
	std::vector<int32_t> rootNodes;
	//rendering order 
	std::vector<size_t> opaqueMeshIndices;
	std::vector<size_t> transparentMeshIndices;
	// GPU buffers
	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
	VkBuffer indexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
};

class ObjectLoader {
public:
	ObjectLoader() = default;
	~ObjectLoader();
	
	void init(Device* device, TextureManager* textureManager, BufferManager* bufferManager);
	void cleanup();
	
	bool loadGLTF(const std::string& filepath, Model& outModel);
	void createModelBuffers(Model& model);
	void destroyModel(Model& model);

private:
	Device* device = nullptr;
	TextureManager* textureManager = nullptr;
	BufferManager* bufferManager = nullptr;
	
	// Store the base path for resolving relative texture paths
	std::string basePath;
	
	void loadNode(const tinygltf::Model& gltfModel, const tinygltf::Node& gltfNode, 
	              int nodeIndex, Model& model, const glm::mat4& parentTransform);
	void loadMesh(const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh,
		Model& model, const glm::mat4& worldTransform);
	void loadMaterials(const tinygltf::Model& gltfModel, Model& model);
	void loadTextures(const tinygltf::Model& gltfModel, Model& model);
	
	// Texture loading helpers
	void loadTextureFromGLTF(const tinygltf::Model& gltfModel, int textureIndex, 
	                         LoadedTexture& outTexture);
	void createTextureFromBuffer(const unsigned char* buffer, int width, int height, 
	                             int channels, LoadedTexture& outTexture);
	VkSamplerAddressMode getVkWrapMode(int wrapMode);
	VkFilter getVkFilterMode(int filterMode);
	
	glm::mat4 getNodeTransform(const tinygltf::Node& node);
};