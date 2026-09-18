#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tiny_gltf.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <future>
#include <mutex>
#include <ktx.h>
#include "../Core/VkDevice.h"
#include "../objects/vertex.h"

class TextureManager;
class BufferManager;
// Ray tracing vertex (std430-friendly layout)
struct RayTracingVertex {
	glm::vec4 position;
	glm::vec4 normal;
	glm::vec2 texCoord;
	float _pad0;
	float _pad1;
};

// KHR_texture_transform: UV transform for a single texture reference.
// Applied as uv' = R * S * uv + O (scale, then rotate, then offset).
struct TextureTransform {
	glm::vec2 offset = glm::vec2(0.0f);
	float rotation = 0.0f;          // radians, counter-clockwise
	glm::vec2 scale = glm::vec2(1.0f);
	bool active = false;
};

// Material data for PBR rendering
struct Material {
	glm::vec4 baseColorFactor = glm::vec4(1.0f);
	float metallicFactor = 0.0f;
	float roughnessFactor = 1.0f;
	int32_t baseColorTextureIndex = -1;
	int32_t normalTextureIndex = -1;
	int32_t metallicRoughnessTextureIndex = -1;
	int32_t emissiveTextureIndex = -1;
	glm::vec3 emissiveFactor = glm::vec3(0.0f);
	bool isTransparent = false;
	bool isEmissive = false;
	float transmissionFactor = 0.0f;
	float idxReflect = 1.5;
	float alphaCutoff = 0.5f;
	glm::vec3 attenuationColor = glm::vec3(1.0f);

	float attenuationDistance = 1e9F;
	// KHR_materials_volume thickness (approximate ray length through the
	// volume when no thickness texture is available)
	float thicknessFactor = 0.0f;
	int32_t thicknessTextureIndex = -1;
	float dispersion = 0.0f;
	float iridesceneFactor = 0.0f;
	float iridesceneIor = 1.3f;
	float iridesceneThicknessMin = 100.0f;
	float iridesceneThicknessMax = 400.0f;
	int32_t iridescenceThicknessTextureIndex = -1;
	float diffuseTransmissionFactor = 0.0f;
	glm::vec3 diffuseTransmissionColor = glm::vec3(1.0f);
	int32_t diffuseTransmissionTextureIndex = -1;

	glm::vec3 scatteringColor = glm::vec3(1.0f);
	float scatteringDistance = 0.0F;
	float scatteringAnisotropy = 0.0F;
	float scatteringRange = 0.0F;
	// KHR_texture_transform per sampled texture slot
	TextureTransform baseColorUvTransform;
	TextureTransform metallicRoughnessUvTransform;
	TextureTransform emissiveUvTransform;
	TextureTransform iridescenceThicknessUvTransform;
	TextureTransform diffuseTransmissionUvTransform;
};

// A single mesh primitive (submesh)
struct Primitive {
	uint32_t firstIndex;
	uint32_t indexCount;
	uint32_t firstVertex;
	uint32_t vertexCount;
	int32_t materialIndex = -1;
	std::vector<int32_t> variantMaterials {};
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
	uint32_t mipLevels = 1;
};
struct GpuMeshInstance{
    glm::vec4 rotation;
    glm::vec3 scale;
    glm::vec3 translation;
    uint32_t meshIndex;
};
// Complete loaded model
struct Model {
	std::vector<Vertex> vertices;
	std::vector<RayTracingVertex> rtVertices;
	std::vector<uint32_t> indices;
	std::vector<Mesh> meshes;
	std::vector<Node> nodes;
	std::vector<Material> materials;
	std::vector<LoadedTexture> textures;
	std::vector<int32_t> rootNodes;
	std::vector<std::string> variantNames;
	int32_t activeVariantIndex = -1;
	//rendering order
	std::vector<size_t> opaqueMeshIndices;
	std::vector<size_t> transparentMeshIndices;
	// MSFT_texture_dds / KTX: per-texture path to a KTX/DDS file (empty = none)
	std::vector<std::string> ktxTexturePaths;
	GpuMeshInstance* gpuMeshInstances = nullptr;

	// GPU buffers
	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
	VkBuffer indexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
	VkBuffer rtVertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory rtVertexBufferMemory = VK_NULL_HANDLE;

};
// Resolves the material a primitive renders with, honoring the currently
// selected KHR_materials_variants variant. Falls back to the base material.
inline int32_t resolveMaterialIndex(const Model& model, const Primitive& prim)
{
	if (model.activeVariantIndex >= 0 &&
	    model.activeVariantIndex < static_cast<int32_t>(prim.variantMaterials.size()) &&
	    prim.variantMaterials[model.activeVariantIndex] >= 0) {
		return prim.variantMaterials[model.activeVariantIndex];
	}
	return prim.materialIndex;
}

class ObjectLoader {
public:
	ObjectLoader() = default;
	~ObjectLoader();

	void init(Device* device, TextureManager* textureManager, BufferManager* bufferManager);
	void cleanup();

	bool loadGLTF(const std::string& filepath, Model& outModel);
	std::future<bool> loadGLTFAsync(const std::string& filepath, Model& outModel);
	void createModelBuffers(Model& model);
	void destroyModel(Model& model);

private:
	Device* device = nullptr;
	TextureManager* textureManager = nullptr;
	BufferManager* bufferManager = nullptr;

	std::mutex vulkanMutex;


	void loadNode(const tinygltf::Model& gltfModel, const tinygltf::Node& gltfNode,
	              int nodeIndex, Model& model, const glm::mat4& parentTransform);
	void loadMesh(const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh,
		Model& model, const glm::mat4& worldTransform);
	void loadMaterials(const tinygltf::Model& gltfModel, Model& model);
	void loadTextures(const tinygltf::Model& gltfModel, Model& model,
	                  const std::string& baseDir);

	// Texture loading helpers
	void uploadTextureToGPU(const unsigned char* pixelData, int width, int height,
	                        LoadedTexture& outTexture);
	// Loads a KTX1/KTX2/DDS file through libktx (all mip levels). Returns
	// false if the file or its format is unsupported (caller falls back).
	bool loadTextureWithKtx(const std::string& path, LoadedTexture& outTexture);
	void loadVariants(const tinygltf::Model& gltfModel, Model& model);
	VkSamplerAddressMode getVkWrapMode(int wrapMode);
	VkFilter getVkFilterMode(int filterMode);

	glm::mat4 getNodeTransform(const tinygltf::Node& node);

	// Primitive data collected in parallel, then merged
	struct PrimitiveData {
		std::vector<Vertex> vertices;
		std::vector<RayTracingVertex> rtVertices;
		std::vector<uint32_t> indices;
		uint32_t vertexCount;
		uint32_t indexCount;
	};
	PrimitiveData loadPrimitiveData(const tinygltf::Model& gltfModel,
	                                const tinygltf::Primitive& primitive,
	                                const glm::mat4& worldTransform,
	                                const glm::mat3& normalMatrix);
};
