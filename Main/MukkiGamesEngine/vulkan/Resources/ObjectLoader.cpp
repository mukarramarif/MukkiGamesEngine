#include "ObjectLoader.h"
#include "BufferManager.h"
#include "TextureManager.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <ktxvulkan.h>  // ktxTexture_GetVkFormat etc.

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

namespace {

// Reads a FLOAT accessor (VEC3/VEC4) into out, expanding sparse accessors
// (allowed for EXT_mesh_gpu_instancing attributes).
void readAccessorFloats(const tinygltf::Model &gltfModel,
                        const tinygltf::Accessor &accessor,
                        std::vector<float> &out) {
  const int comps = tinygltf::GetNumComponentsInType(accessor.type);
  out.assign(static_cast<size_t>(accessor.count) * comps, 0.0f);

  if (accessor.bufferView >= 0) {
    const tinygltf::BufferView &bv = gltfModel.bufferViews[accessor.bufferView];
    const float *src = reinterpret_cast<const float *>(
        &gltfModel.buffers[bv.buffer]
             .data[accessor.byteOffset + bv.byteOffset]);
    std::copy(src, src + out.size(), out.begin());
  }

  if (accessor.sparse.isSparse) {
    const tinygltf::BufferView &idxBv =
        gltfModel.bufferViews[accessor.sparse.indices.bufferView];
    const uint8_t *idxBase =
        &gltfModel.buffers[idxBv.buffer]
             .data[accessor.sparse.indices.byteOffset + idxBv.byteOffset];
    const tinygltf::BufferView &valBv =
        gltfModel.bufferViews[accessor.sparse.values.bufferView];
    const float *valBase = reinterpret_cast<const float *>(
        &gltfModel.buffers[valBv.buffer]
             .data[accessor.sparse.values.byteOffset + valBv.byteOffset]);

    for (int s = 0; s < accessor.sparse.count; ++s) {
      uint32_t index = 0;
      switch (accessor.sparse.indices.componentType) {
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        index = idxBase[s];
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        index = reinterpret_cast<const uint16_t *>(idxBase)[s];
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        index = reinterpret_cast<const uint32_t *>(idxBase)[s];
        break;
      default:
        continue;
      }
      for (int c = 0; c < comps; ++c)
        out[static_cast<size_t>(index) * comps + c] =
            valBase[static_cast<size_t>(s) * comps + c];
    }
  }
}

// KHR_texture_transform: parses the transform object itself.
void parseTextureTransformObject(const tinygltf::Value &t, TextureTransform &out) {
  if (t.Has("offset")) {
    out.offset = glm::vec2(
        static_cast<float>(t.Get("offset").Get(0).Get<double>()),
        static_cast<float>(t.Get("offset").Get(1).Get<double>()));
  }
  if (t.Has("rotation")) {
    out.rotation = static_cast<float>(t.Get("rotation").Get<double>());
  }
  if (t.Has("scale")) {
    out.scale = glm::vec2(
        static_cast<float>(t.Get("scale").Get(0).Get<double>()),
        static_cast<float>(t.Get("scale").Get(1).Get<double>()));
  }
  out.active = true;
}

// texInfo = tinygltf Parameter of a texture-info object
// (e.g. baseColorTexture / metallicRoughnessTexture in material.values).
// Transforms live on the parsed TextureInfo struct instead.
void parseTextureTransform(const tinygltf::TextureInfo &texInfo,
                           TextureTransform &out) {
  auto it = texInfo.extensions.find("KHR_texture_transform");
  if (it != texInfo.extensions.end()) {
    parseTextureTransformObject(it->second, out);
  }
}

// texInfo = tinygltf Value of a texture-info object stored inside a material
// extension (e.g. iridescenceThicknessTexture / diffuseTransmissionTexture).
void parseTextureTransform(const tinygltf::Value &texInfo, TextureTransform &out) {
  if (!texInfo.Has("extensions"))
    return;
  const tinygltf::Value &exts = texInfo.Get("extensions");
  if (exts.Has("KHR_texture_transform")) {
    parseTextureTransformObject(exts.Get("KHR_texture_transform"), out);
  }
}

} // namespace

ObjectLoader::~ObjectLoader() { cleanup(); }

void ObjectLoader::init(Device *device, TextureManager *textureManager,
                        BufferManager *bufferManager) {
  this->device = device;
  this->textureManager = textureManager;
  this->bufferManager = bufferManager;
}

void ObjectLoader::cleanup() {}

std::future<bool> ObjectLoader::loadGLTFAsync(const std::string &filepath,
                                              Model &outModel) {
  return std::async(std::launch::async, [this, filepath, &outModel]() {
    return loadGLTF(filepath, outModel);
  });
}

bool ObjectLoader::loadGLTF(const std::string &filepath, Model &outModel) {
  tinygltf::Model gltfModel;
  tinygltf::TinyGLTF loader;
  std::string err, warn;

  	bool result = false;

  	// Base directory for resolving relative asset paths
  	const std::string baseDir =
  		filepath.substr(0, filepath.find_last_of("/\\") + 1);

  	if (filepath.find(".glb") != std::string::npos) {
  		result = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filepath);
  	} else {
  		// MSFT_texture_dds / DDS images are not decodable by tinygltf, so
  		// preprocess the JSON: record the KTX/DDS variants for later loading
  		// through libktx and strip them from the document tinygltf parses.
  		std::ifstream ifs(filepath, std::ios::binary);
  		std::string jsonText((std::istreambuf_iterator<char>(ifs)),
  		                     std::istreambuf_iterator<char>());
  		ifs.close();

  		outModel.ktxTexturePaths.clear();
  		try {
  			nlohmann::json j = nlohmann::json::parse(jsonText);
  			if (j.contains("images") && j["images"].is_array()) {
  				auto &images = j["images"];
  				std::vector<bool> isDds(images.size(), false);
  				std::vector<std::string> imageDdsPath(images.size());

  				for (size_t i = 0; i < images.size(); ++i) {
  					auto &img = images[i];
  					if (img.contains("extensions") &&
  					    img["extensions"].contains("MSFT_texture_dds")) {
  						int ddsSource =
  						    img["extensions"]["MSFT_texture_dds"].value("source", -1);
  						if (ddsSource >= 0 &&
  						    ddsSource < static_cast<int>(images.size())) {
  							isDds[ddsSource] = true;
  							imageDdsPath[i] =
  							    images[ddsSource].value("uri", std::string());
  						}
  						img["extensions"].erase("MSFT_texture_dds");
  						if (img["extensions"].empty())
  							img.erase("extensions");
  					}
  				}

				// Texture index -> DDS path (recorded before renumbering), and
				// the set of images referenced by any texture's source.
				std::vector<bool> referenced(images.size(), false);
				if (j.contains("textures") && j["textures"].is_array()) {
					outModel.ktxTexturePaths.resize(j["textures"].size());
					for (size_t t = 0; t < j["textures"].size(); ++t) {
						const auto &tex = j["textures"][t];
						if (!tex.contains("source"))
							continue;
						int s = tex["source"].get<int>();
						if (s >= 0 && s < static_cast<int>(images.size())) {
							referenced[s] = true;
							outModel.ktxTexturePaths[t] = imageDdsPath[s];
						}
					}
				}

				// Drop the DDS images and renumber the remaining sources.
				// Any unreferenced .dds file is removed too (e.g. splash
				// images): tinygltf cannot decode DDS and a single failure
				// poisons the whole load.
				std::vector<int> remap(images.size(), -1);
				nlohmann::json newImages = nlohmann::json::array();
				for (size_t i = 0; i < images.size(); ++i) {
					if (isDds[i])
						continue;
					const std::string uri = images[i].value("uri", std::string());
					const bool isDdsFile =
					    uri.size() >= 4 && uri.compare(uri.size() - 4, 4, ".dds") == 0;
					if (isDdsFile && !referenced[i])
						continue;
					remap[i] = static_cast<int>(newImages.size());
					newImages.push_back(images[i]);
				}
				j["images"] = newImages;

  				if (j.contains("textures") && j["textures"].is_array()) {
  					for (auto &tex : j["textures"]) {
  						if (!tex.contains("source"))
  							continue;
  						int oldSrc = tex["source"].get<int>();
  						if (oldSrc >= 0 && oldSrc < static_cast<int>(remap.size()) &&
  						    remap[oldSrc] >= 0)
  							tex["source"] = remap[oldSrc];
  					}
  				}
  			}
  			jsonText = j.dump();
  		} catch (const std::exception &e) {
  			std::cerr << "glTF preprocessing failed (" << e.what()
  			          << "); falling back to raw parse" << std::endl;
  		}

  		result = loader.LoadASCIIFromString(&gltfModel, &err, &warn,
  		                                    jsonText.c_str(),
  		                                    static_cast<unsigned int>(jsonText.size()),
  		                                    baseDir);
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

  	loadTextures(gltfModel, outModel, baseDir);
  loadMaterials(gltfModel, outModel);
  loadVariants(gltfModel, outModel);
  if (!outModel.variantNames.empty()) {
    std::cout << "  Variants: " << outModel.variantNames.size();
    for (const auto &name : outModel.variantNames) {
      std::cout << " [" << name << "]";
    }
    std::cout << std::endl;
  }
  outModel.nodes.resize(gltfModel.nodes.size());

  const tinygltf::Scene &scene =
      gltfModel
          .scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];

  for (int nodeIndex : scene.nodes) {
    outModel.rootNodes.push_back(nodeIndex);
    loadNode(gltfModel, gltfModel.nodes[nodeIndex], nodeIndex, outModel,
             glm::mat4(1.0f));
  }

  std::cout << "  Loaded vertices: " << outModel.vertices.size() << std::endl;
  std::cout << "  Loaded indices: " << outModel.indices.size() << std::endl;
  std::cout << "  Loaded textures: " << outModel.textures.size() << std::endl;
  for (size_t i = 0; i < outModel.meshes.size(); i++) {
    bool hasTransparent = false;
    for (const auto &prim : outModel.meshes[i].primitives) {
      if (prim.materialIndex >= 0 &&
          prim.materialIndex <
              static_cast<int32_t>(outModel.materials.size()) &&
          outModel.materials[prim.materialIndex].isTransparent) {
        hasTransparent = true;
        break;
      }
    }

    if (hasTransparent) {
      outModel.transparentMeshIndices.push_back(i);
    } else {
      outModel.opaqueMeshIndices.push_back(i);
    }
  }
  return true;
}

void ObjectLoader::loadTextures(const tinygltf::Model &gltfModel,
                                Model &model, const std::string &baseDir) {
  size_t textureCount = gltfModel.textures.size();
  model.textures.resize(textureCount);

  // Phase 1: Convert pixel data in parallel
  std::vector<std::vector<unsigned char>> convertedBuffers(textureCount);
  std::vector<int> widths(textureCount);
  std::vector<int> heights(textureCount);
  std::vector<std::future<void>> conversions;

  for (size_t i = 0; i < textureCount; i++) {
    const tinygltf::Texture &gltfTexture = gltfModel.textures[i];
    const tinygltf::Image &gltfImage = gltfModel.images[gltfTexture.source];

    if (gltfImage.image.empty() || gltfImage.width == 0 ||
        gltfImage.height == 0) {
      continue;
    }

    widths[i] = gltfImage.width;
    heights[i] = gltfImage.height;

    conversions.push_back(
        std::async(std::launch::async, [&gltfImage, &convertedBuffers, i]() {
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

  for (auto &f : conversions) {
    f.get();
  }

  	// Phase 2: Upload textures to GPU sequentially
  	for (size_t i = 0; i < textureCount; i++) {
  		const tinygltf::Texture &gltfTexture = gltfModel.textures[i];
  		LoadedTexture &outTexture = model.textures[i];

  		// Prefer the KTX/DDS variant (MSFT_texture_dds) loaded through libktx:
  		// BC-compressed, smaller on the GPU, and carries a full mip chain.
  		bool loadedViaKtx = false;
  		if (i < model.ktxTexturePaths.size() &&
  		    !model.ktxTexturePaths[i].empty()) {
  			loadedViaKtx =
  			    loadTextureWithKtx(baseDir + model.ktxTexturePaths[i], outTexture);
  		}

  		if (!loadedViaKtx) {
  			const tinygltf::Image &gltfImage = gltfModel.images[gltfTexture.source];
  			if (gltfImage.image.empty() || gltfImage.width == 0 ||
  			    gltfImage.height == 0) {
  				std::cerr << "Invalid image data for texture " << i << std::endl;
  				continue;
  			}

  			int channels = gltfImage.component;
  			outTexture.width = static_cast<uint32_t>(widths[i]);
  			outTexture.height = static_cast<uint32_t>(heights[i]);

  			const unsigned char *pixelData =
  			    channels == 4 ? gltfImage.image.data() : convertedBuffers[i].data();

  			uploadTextureToGPU(pixelData, widths[i], heights[i], outTexture);
  		}

  		VkSamplerCreateInfo samplerInfo{};
  		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    if (gltfTexture.sampler >= 0 &&
        gltfTexture.sampler < static_cast<int>(gltfModel.samplers.size())) {
      const tinygltf::Sampler &gltfSampler =
          gltfModel.samplers[gltfTexture.sampler];
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
    		// KTX/DDS textures carry a full mip chain
    		samplerInfo.maxLod =
    		    static_cast<float>(outTexture.mipLevels > 1 ? outTexture.mipLevels - 1 : 0);

    {
      std::lock_guard<std::mutex> lock(vulkanMutex);
      if (vkCreateSampler(device->getDevice(), &samplerInfo, nullptr,
                          &outTexture.sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
      }
    }
  }
}

void ObjectLoader::uploadTextureToGPU(const unsigned char *pixelData, int width,
                                      int height, LoadedTexture &outTexture) {
  std::lock_guard<std::mutex> lock(vulkanMutex);

  VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  bufferManager->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              stagingBuffer, stagingBufferMemory);

  void *data;
  vkMapMemory(device->getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
  memcpy(data, pixelData, static_cast<size_t>(imageSize));
  vkUnmapMemory(device->getDevice(), stagingBufferMemory);

  textureManager->createImage(
      static_cast<uint32_t>(width), static_cast<uint32_t>(height),
      VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outTexture.image, outTexture.memory);

  textureManager->transitionImageLayout(
      outTexture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  textureManager->copyBufferToImage(stagingBuffer, outTexture.image,
                                    static_cast<uint32_t>(width),
                                    static_cast<uint32_t>(height));

  textureManager->transitionImageLayout(
      outTexture.image, VK_FORMAT_R8G8B8A8_SRGB,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  outTexture.imageView = textureManager->createImageView(
      outTexture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

  	bufferManager->destroyBuffer(stagingBuffer, stagingBufferMemory);
}

bool ObjectLoader::loadTextureWithKtx(const std::string &path,
                                      LoadedTexture &outTexture) {
	ktxTexture *texture = nullptr;
	KTX_error_code result = ktxTexture_CreateFromNamedFile(
	    path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
	if (result != KTX_SUCCESS || !texture)
		return false;

	VkFormat format = ktxTexture_GetVkFormat(texture);
	if (format == VK_FORMAT_UNDEFINED) {
		ktxTexture_Destroy(texture);
		return false;
	}

	// The format must be sampleable with optimal tiling on this device
	VkFormatProperties props{};
	vkGetPhysicalDeviceFormatProperties(device->getPhysicalDevice(), format, &props);
	if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
		ktxTexture_Destroy(texture);
		return false;
	}

	const uint32_t width = texture->baseWidth;
	const uint32_t height = texture->baseHeight;
	const uint32_t mipLevels = texture->numLevels;
	if (width == 0 || height == 0 || mipLevels == 0) {
		ktxTexture_Destroy(texture);
		return false;
	}

	const ktx_size_t totalSize = ktxTexture_GetDataSize(texture);
	const ktx_uint8_t *srcData = ktxTexture_GetData(texture);
	const bool blockCompressed = texture->isCompressed == KTX_TRUE;

	// Everything from here touches Vulkan resources; async model loads can
	// run concurrently, so take the same mutex as uploadTextureToGPU.
	std::lock_guard<std::mutex> lock(vulkanMutex);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	bufferManager->createBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
	                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	                            stagingBuffer, stagingMemory);

	void *mapped = nullptr;
	vkMapMemory(device->getDevice(), stagingMemory, 0, totalSize, 0, &mapped);

	std::vector<VkBufferImageCopy> regions(mipLevels);
	VkDeviceSize running = 0;
	for (uint32_t level = 0; level < mipLevels; ++level) {
		ktx_size_t offset = 0;
		if (ktxTexture_GetImageOffset(texture, level, 0, 0, &offset) != KTX_SUCCESS) {
			vkUnmapMemory(device->getDevice(), stagingMemory);
			bufferManager->destroyBuffer(stagingBuffer, stagingMemory);
			ktxTexture_Destroy(texture);
			return false;
		}
		const ktx_size_t size = ktxTexture_GetImageSize(texture, level);
		memcpy(static_cast<char *>(mapped) + running, srcData + offset,
		       static_cast<size_t>(size));

		uint32_t mipW = std::max(1u, width >> level);
		uint32_t mipH = std::max(1u, height >> level);
		if (blockCompressed) {
			// Block-compressed (BCn/ETC2) mip extents must be multiples of 4
			mipW = std::max(4u, ((mipW + 3) / 4) * 4);
			mipH = std::max(4u, ((mipH + 3) / 4) * 4);
		}

		regions[level] = {};
		regions[level].bufferOffset = running;
		regions[level].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		regions[level].imageSubresource.mipLevel = level;
		regions[level].imageSubresource.baseArrayLayer = 0;
		regions[level].imageSubresource.layerCount = 1;
		regions[level].imageExtent = {mipW, mipH, 1};
		running += size;
	}
	vkUnmapMemory(device->getDevice(), stagingMemory);

	textureManager->createImage(
	    width, height, format, VK_IMAGE_TILING_OPTIMAL,
	    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
	    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outTexture.image,
	    outTexture.memory, false, mipLevels);
	textureManager->transitionImageLayout(
	    outTexture.image, format, VK_IMAGE_LAYOUT_UNDEFINED,
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, false, mipLevels);
	textureManager->copyBufferToImageRegions(stagingBuffer, outTexture.image,
	                                         regions);
	textureManager->transitionImageLayout(
	    outTexture.image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false, mipLevels);
	outTexture.imageView = textureManager->createImageView(
	    outTexture.image, format, VK_IMAGE_ASPECT_COLOR_BIT, false, mipLevels);

	bufferManager->destroyBuffer(stagingBuffer, stagingMemory);
	ktxTexture_Destroy(texture);

	outTexture.width = width;
	outTexture.height = height;
	outTexture.mipLevels = mipLevels;
	return true;
}

VkSamplerAddressMode ObjectLoader::getVkWrapMode(int wrapMode) {
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

VkFilter ObjectLoader::getVkFilterMode(int filterMode) {
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

void ObjectLoader::loadMaterials(const tinygltf::Model &gltfModel,
                                 Model &model) {
  model.materials.reserve(gltfModel.materials.size());
  for (const auto &gltfMaterial : gltfModel.materials) {
    Material material;
    material.isTransparent = (gltfMaterial.alphaMode == "BLEND");

    if (gltfMaterial.alphaMode == "MASK") {
      material.alphaCutoff = static_cast<float>(gltfMaterial.alphaCutoff);
    }
    // PBR Metallic Roughness workflow
    if (gltfMaterial.values.find("baseColorFactor") !=
        gltfMaterial.values.end()) {
      material.baseColorFactor = glm::make_vec4(
          gltfMaterial.values.at("baseColorFactor").ColorFactor().data());
    }
    if (gltfMaterial.values.find("metallicRoughnessTexture") !=
        gltfMaterial.values.end()) {
      const tinygltf::Parameter &texInfo =
          gltfMaterial.values.at("metallicRoughnessTexture");
      material.metallicRoughnessTextureIndex = texInfo.TextureIndex();
      parseTextureTransform(gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture,
                            material.metallicRoughnessUvTransform);
    }

    if (gltfMaterial.values.find("metallicFactor") !=
        gltfMaterial.values.end()) {
      material.metallicFactor =
          static_cast<float>(gltfMaterial.values.at("metallicFactor").Factor());
    }
    if (gltfMaterial.values.find("roughnessFactor") !=
        gltfMaterial.values.end()) {
      material.roughnessFactor = static_cast<float>(
          gltfMaterial.values.at("roughnessFactor").Factor());
    }
    	if (gltfMaterial.values.find("baseColorTexture") !=
    	    gltfMaterial.values.end()) {
    		const tinygltf::Parameter &texInfo =
    		    gltfMaterial.values.at("baseColorTexture");
    		material.baseColorTextureIndex = texInfo.TextureIndex();
    		parseTextureTransform(gltfMaterial.pbrMetallicRoughness.baseColorTexture,
    		                      material.baseColorUvTransform);
    	}

    	// KHR_materials_pbrSpecularGlossiness (Bistro uses this): convert to the
    	// metallic-roughness model (approximation, three.js-style).
    	if (gltfMaterial.extensions.contains("KHR_materials_pbrSpecularGlossiness")) {
    		const auto &sg = gltfMaterial.extensions.at("KHR_materials_pbrSpecularGlossiness");
    		if (sg.Has("diffuseFactor")) {
    			const auto &c = sg.Get("diffuseFactor");
    			material.baseColorFactor = glm::vec4(
    			    static_cast<float>(c.Get(0).Get<double>()),
    			    static_cast<float>(c.Get(1).Get<double>()),
    			    static_cast<float>(c.Get(2).Get<double>()),
    			    static_cast<float>(c.Get(3).Get<double>()));
    		}
    		float specLum = 1.0f;
    		if (sg.Has("specularFactor")) {
    			const auto &c = sg.Get("specularFactor");
    			float r = static_cast<float>(c.Get(0).Get<double>());
    			float g = static_cast<float>(c.Get(1).Get<double>());
    			float b = static_cast<float>(c.Get(2).Get<double>());
    			specLum = std::max(r, std::max(g, b));
    		}
    		if (sg.Has("glossinessFactor")) {
    			float gloss =
    			    static_cast<float>(sg.Get("glossinessFactor").Get<double>());
    			material.roughnessFactor = 1.0f - gloss;
    		}
    		material.metallicFactor = glm::clamp(specLum, 0.0f, 1.0f);
    		material.baseColorFactor.r *= (1.0f - material.metallicFactor);
    		material.baseColorFactor.g *= (1.0f - material.metallicFactor);
    		material.baseColorFactor.b *= (1.0f - material.metallicFactor);
    		if (sg.Has("diffuseTexture")) {
    			const auto &t = sg.Get("diffuseTexture");
    			if (t.Has("index"))
    				material.baseColorTextureIndex = t.Get("index").Get<int>();
    		}
    		if (sg.Has("specularGlossinessTexture")) {
    			const auto &t = sg.Get("specularGlossinessTexture");
    			if (t.Has("index"))
    				material.metallicRoughnessTextureIndex = t.Get("index").Get<int>();
    		}
    	}

    	model.materials.push_back(material);
  }
  // Process emissive properties
  for (size_t i = 0; i < gltfModel.materials.size(); i++) {
    const auto &gltfMat = gltfModel.materials[i];
    Material &mat = model.materials[i];
    if (!gltfMat.emissiveFactor.empty()) {
      mat.emissiveFactor =
          glm::vec3(gltfMat.emissiveFactor[0], gltfMat.emissiveFactor[1],
                    gltfMat.emissiveFactor[2]);
    }

    if (gltfMat.emissiveTexture.index >= 0) {
      mat.emissiveTextureIndex = gltfMat.emissiveTexture.index;
      parseTextureTransform(gltfMat.emissiveTexture, mat.emissiveUvTransform);
    }

    // Mark as emissive if it has emissive texture or non-zero emissive factor
    // Also check material name for "lightflare" or similar
    mat.isEmissive = (mat.emissiveTextureIndex >= 0) ||
                     (glm::length(mat.emissiveFactor) > 0.01f) ||
                     (gltfMat.name.find("lightflare") != std::string::npos) ||
                     (gltfMat.name.find("flare") != std::string::npos) ||
                     (gltfMat.name.find("glow") != std::string::npos) ||
                     (gltfMat.name.find("emissive") != std::string::npos);
    // making emissive textures transparent
    if (mat.isEmissive) {
      mat.isTransparent = true;
    }
  }
  // process Transmission properties
  for (size_t i = 0; i < gltfModel.materials.size(); i++) {
    const auto &gltfMat = gltfModel.materials[i];
    Material &mat = model.materials[i];

    if (gltfMat.extensions.find("KHR_materials_transmission") !=
        gltfMat.extensions.end()) {
      const auto &transmissionExt =
          gltfMat.extensions.at("KHR_materials_transmission");
      if (transmissionExt.Has("transmissionFactor")) {
        mat.transmissionFactor = static_cast<float>(
            transmissionExt.Get("transmissionFactor").GetNumberAsDouble());
      }
    }
    // KHR_materials_transmission → glass
    if (gltfMat.extensions.contains("KHR_materials_transmission")) {
      const auto &ext = gltfMat.extensions.at("KHR_materials_transmission");
      if (ext.Has("transmissionFactor")) {
        mat.transmissionFactor =
            static_cast<float>(ext.Get("transmissionFactor").Get<double>());
      }
      // Glass renders through the transparent pass (fresnel alpha blend),
      // regardless of alphaMode.
      if (mat.transmissionFactor > 0.0f) {
        mat.isTransparent = true;
      }
    }
    if (gltfMat.extensions.contains("KHR_materials_volume")) {
      const auto &ext = gltfMat.extensions.at("KHR_materials_volume");
      if (ext.Has("attenuationColor")) {
        const auto &c = ext.Get("attenuationColor");
        mat.attenuationColor =
            glm::vec3(static_cast<float>(c.Get(0).Get<double>()),
                      static_cast<float>(c.Get(1).Get<double>()),
                      static_cast<float>(c.Get(2).Get<double>()));
      }
      if (ext.Has("attenuationDistance")) {
        mat.attenuationDistance =
            static_cast<float>(ext.Get("attenuationDistance").Get<double>());
      }
      if (ext.Has("thicknessFactor")) {
        mat.thicknessFactor =
            static_cast<float>(ext.Get("thicknessFactor").Get<double>());
      }
      if (ext.Has("thicknessTexture")) {
        const auto &tex = ext.Get("thicknessTexture");
        if (tex.Has("index")) {
          mat.thicknessTextureIndex =
              static_cast<int>(tex.Get("index").Get<double>());
        }
      }
    }
    if (gltfMat.extensions.contains("KHR_materials_diffuse_transmission")) {
      const auto &ext =
          gltfMat.extensions.at("KHR_materials_diffuse_transmission");
      if (ext.Has("diffuseTransmissionFactor")) {
        mat.diffuseTransmissionFactor = static_cast<float>(
            ext.Get("diffuseTransmissionFactor").Get<double>());
      }
      if (ext.Has("diffuseTransmissionColorFactor")) {
        const auto &c = ext.Get("diffuseTransmissionColorFactor");
        mat.diffuseTransmissionColor =
            glm::vec3(static_cast<float>(c.Get(0).Get<double>()),
                      static_cast<float>(c.Get(1).Get<double>()),
                      static_cast<float>(c.Get(2).Get<double>()));
      }
      if (ext.Has("diffuseTransmissionTexture")) {
        const auto &tex = ext.Get("diffuseTransmissionTexture");
        if (tex.Has("index")) {
          mat.diffuseTransmissionTextureIndex =
              static_cast<int>(tex.Get("index").Get<double>());
        }
        parseTextureTransform(tex, mat.diffuseTransmissionUvTransform);
      }
    }
    if (gltfMat.extensions.contains("KHR_materials_volume_scatter")) {
      const auto &ext = gltfMat.extensions.at("KHR_materials_volume_scatter");
      if (ext.Has("multiscatterColor")) {
        const auto &c = ext.Get("multiscatterColor");
        mat.scatteringColor =
            glm::vec3(static_cast<float>(c.Get(0).Get<double>()),
                      static_cast<float>(c.Get(1).Get<double>()),
                      static_cast<float>(c.Get(2).Get<double>()));
      }
      if (ext.Has("multiscatterDistance")) {
        mat.scatteringDistance =
            static_cast<float>(ext.Get("multiscatterDistance").Get<double>());
      }
      if (ext.Has("multiscatterAnisotropy")) {
        mat.scatteringAnisotropy =
            static_cast<float>(ext.Get("multiscatterAnisotropy").Get<double>());
      }
      if (ext.Has("multiscatterRange")) {
        mat.scatteringRange =
            static_cast<float>(ext.Get("multiscatterRange").Get<double>());
      }
    }
    // KHR_materials_ior → index of refraction
    if (gltfMat.extensions.contains("KHR_materials_ior")) {
      const auto &ext = gltfMat.extensions.at("KHR_materials_ior");
      if (ext.Has("ior")) {
        mat.idxReflect = static_cast<float>(ext.Get("ior").Get<double>());
      }
    }
    // KHR_materials_emissive_strength → fold into the emissive factor
    if (gltfMat.extensions.contains("KHR_materials_emissive_strength")) {
      const auto &ext =
          gltfMat.extensions.at("KHR_materials_emissive_strength");
      if (ext.Has("emissiveStrength")) {
        float strength =
            static_cast<float>(ext.Get("emissiveStrength").Get<double>());
        mat.emissiveFactor *= strength;
      }
    }
    if (gltfMat.extensions.contains("KHR_materials_dispersion")) {
      const auto &ext = gltfMat.extensions.at("KHR_materials_dispersion");
      if (ext.Has("dispersion")) {
        mat.dispersion =
            static_cast<float>(ext.Get("dispersion").Get<double>());
      }
    }
    if (gltfMat.extensions.contains("KHR_materials_iridescence")) {
      const auto &ext = gltfMat.extensions.at("KHR_materials_iridescence");
      if (ext.Has("iridescenceFactor")) {
        mat.iridesceneFactor =
            static_cast<float>(ext.Get("iridescenceFactor").Get<double>());
      }
      if (ext.Has("iridescenceIor")) {
        mat.iridesceneIor =
            static_cast<float>(ext.Get("iridescenceIor").Get<double>());
      }
      if (ext.Has("iridescenceThicknessMinimum")) {
        mat.iridesceneThicknessMin = static_cast<float>(
            ext.Get("iridescenceThicknessMinimum").Get<double>());
      }
      if (ext.Has("iridescenceThicknessMaximum")) {
        mat.iridesceneThicknessMax = static_cast<float>(
            ext.Get("iridescenceThicknessMaximum").Get<double>());
      }
      if (ext.Has("iridescenceThicknessTexture")) {
        const auto &tex = ext.Get("iridescenceThicknessTexture");
        if (tex.Has("index")) {
          mat.iridescenceThicknessTextureIndex =
              static_cast<int>(tex.Get("index").Get<double>());
        }
        parseTextureTransform(tex, mat.iridescenceThicknessUvTransform);
      }
    }
  }

  // Add default material if none exist
  if (model.materials.empty()) {
    model.materials.push_back(Material{});
  }
}
void ObjectLoader::loadVariants(const tinygltf::Model &gltfModel,
                                Model &model) {
  auto it = gltfModel.extensions.find("KHR_materials_variants");
  if (it == gltfModel.extensions.end()) {
    return;
  }

  const tinygltf::Value &ext = it->second;
  if (!ext.Has("variants")) {
    return;
  }

  const tinygltf::Value &variants = ext.Get("variants");
  for (size_t i = 0; i < variants.ArrayLen(); ++i) {
    const tinygltf::Value &v = variants.Get(i);
    std::string name;
    if (v.Has("name")) {
      name = v.Get("name").Get<std::string>();
    }
    model.variantNames.push_back(name);
  }
}

void ObjectLoader::loadNode(const tinygltf::Model &gltfModel,
                            const tinygltf::Node &gltfNode, int nodeIndex,
                            Model &model, const glm::mat4 &parentTransform) {
  Node &node = model.nodes[nodeIndex];
  node.name = gltfNode.name;
  node.localTransform = getNodeTransform(gltfNode);
  node.worldTransform = parentTransform * node.localTransform;

  // Load mesh if present - pass the world transform to apply to vertices
  if (gltfNode.mesh > -1) {
    node.meshIndex = static_cast<int32_t>(model.meshes.size());

    const auto instIt = gltfNode.extensions.find("EXT_mesh_gpu_instancing");
    if (instIt != gltfNode.extensions.end()) {
      // EXT_mesh_gpu_instancing: TRANSLATION / ROTATION / SCALE accessors,
      // one element per instance. Flatten at load time: one baked mesh +
      // one synthetic node per instance, so the rasterizer and the RT
      // TLAS (which iterates nodes) both place every instance.
      const tinygltf::Value &ext = instIt->second;
      std::vector<float> translations, rotations, scales;
      size_t instanceCount = 0;

      if (ext.Has("attributes")) {
        const tinygltf::Value &attrs = ext.Get("attributes");
        if (attrs.Has("TRANSLATION")) {
          const int acc =
              static_cast<int>(attrs.Get("TRANSLATION").Get<double>());
          readAccessorFloats(gltfModel, gltfModel.accessors[acc], translations);
          instanceCount = translations.size() / 3;
        }
        if (attrs.Has("ROTATION")) {
          const int acc = static_cast<int>(attrs.Get("ROTATION").Get<double>());
          readAccessorFloats(gltfModel, gltfModel.accessors[acc], rotations);
          if (instanceCount == 0)
            instanceCount = rotations.size() / 4;
        }
        if (attrs.Has("SCALE")) {
          const int acc = static_cast<int>(attrs.Get("SCALE").Get<double>());
          readAccessorFloats(gltfModel, gltfModel.accessors[acc], scales);
          if (instanceCount == 0)
            instanceCount = scales.size() / 3;
        }
      }

      if (instanceCount == 0) {
        // Empty or malformed extension: render the mesh once.
        loadMesh(gltfModel, gltfModel.meshes[gltfNode.mesh], model,
                 node.worldTransform);
      } else {
        // The node itself owns no drawn mesh; each synthetic child
        // carries one flattened instance.
        node.meshIndex = -1;

        // model.nodes may reallocate on push_back: capture the node
        // transform and first synthetic index up front, and never use
        // the 'node' reference again after the pushes.
        const glm::mat4 nodeWorld = node.worldTransform;
        const int32_t firstInstanceIndex =
            static_cast<int32_t>(model.nodes.size());

        for (size_t i = 0; i < instanceCount; ++i) {
          glm::mat4 instanceTransform = glm::mat4(1.0f);
          if (!translations.empty()) {
            instanceTransform = glm::translate(
                instanceTransform,
                glm::vec3(translations[i * 3 + 0], translations[i * 3 + 1],
                          translations[i * 3 + 2]));
          }
          if (!rotations.empty()) {
            instanceTransform =
                instanceTransform *
                glm::mat4_cast(
                    glm::quat(rotations[i * 4 + 3], rotations[i * 4 + 0],
                              rotations[i * 4 + 1], rotations[i * 4 + 2]));
          }
          if (!scales.empty()) {
            instanceTransform =
                glm::scale(instanceTransform,
                           glm::vec3(scales[i * 3 + 0], scales[i * 3 + 1],
                                     scales[i * 3 + 2]));
          }

          Node instanceNode;
          instanceNode.name = gltfNode.name + "_instance_" + std::to_string(i);
          instanceNode.localTransform = instanceTransform;
          instanceNode.worldTransform = nodeWorld * instanceTransform;
          instanceNode.meshIndex = static_cast<int32_t>(model.meshes.size());
          instanceNode.parent = nodeIndex;
          model.nodes.push_back(instanceNode);

          loadMesh(gltfModel, gltfModel.meshes[gltfNode.mesh], model,
                   instanceNode.worldTransform);
        }

        // Register the synthetic children on the parent via a fresh
        // reference (the old one may be dangling after the pushes).
        for (size_t i = 0; i < instanceCount; ++i) {
          model.nodes[nodeIndex].children.push_back(firstInstanceIndex +
                                                    static_cast<int32_t>(i));
        }
      }
    } else {
      loadMesh(gltfModel, gltfModel.meshes[gltfNode.mesh], model,
               node.worldTransform);
    }
  }

  // Process children (re-fetch the node each time: instancing pushes may
  // have reallocated model.nodes, invalidating the earlier reference)
  for (int childIndex : gltfNode.children) {
    model.nodes[nodeIndex].children.push_back(childIndex);
    model.nodes[childIndex].parent = nodeIndex;
    loadNode(gltfModel, gltfModel.nodes[childIndex], childIndex, model,
             model.nodes[nodeIndex].worldTransform);
  }
}

ObjectLoader::PrimitiveData ObjectLoader::loadPrimitiveData(
    const tinygltf::Model &gltfModel, const tinygltf::Primitive &primitive,
    const glm::mat4 &worldTransform, const glm::mat3 &normalMatrix) {
  PrimitiveData result;

  const float *positionBuffer = nullptr;
  const float *normalBuffer = nullptr;
  const float *texCoordBuffer = nullptr;
  const float *colorBuffer = nullptr;
  int colorComponentCount = 3;

  if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
    const tinygltf::Accessor &accessor =
        gltfModel.accessors[primitive.attributes.find("POSITION")->second];
    const tinygltf::BufferView &bufferView =
        gltfModel.bufferViews[accessor.bufferView];
    positionBuffer = reinterpret_cast<const float *>(
        &gltfModel.buffers[bufferView.buffer]
             .data[accessor.byteOffset + bufferView.byteOffset]);
    result.vertexCount = static_cast<uint32_t>(accessor.count);
  }

  if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
    const tinygltf::Accessor &accessor =
        gltfModel.accessors[primitive.attributes.find("NORMAL")->second];
    const tinygltf::BufferView &bufferView =
        gltfModel.bufferViews[accessor.bufferView];
    normalBuffer = reinterpret_cast<const float *>(
        &gltfModel.buffers[bufferView.buffer]
             .data[accessor.byteOffset + bufferView.byteOffset]);
  }

  if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
    const tinygltf::Accessor &accessor =
        gltfModel.accessors[primitive.attributes.find("TEXCOORD_0")->second];
    const tinygltf::BufferView &bufferView =
        gltfModel.bufferViews[accessor.bufferView];
    texCoordBuffer = reinterpret_cast<const float *>(
        &gltfModel.buffers[bufferView.buffer]
             .data[accessor.byteOffset + bufferView.byteOffset]);
  }

  if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
    const tinygltf::Accessor &accessor =
        gltfModel.accessors[primitive.attributes.find("COLOR_0")->second];
    const tinygltf::BufferView &bufferView =
        gltfModel.bufferViews[accessor.bufferView];
    colorBuffer = reinterpret_cast<const float *>(
        &gltfModel.buffers[bufferView.buffer]
             .data[accessor.byteOffset + bufferView.byteOffset]);
    colorComponentCount = (accessor.type == TINYGLTF_TYPE_VEC4) ? 4 : 3;
  }

  result.vertices.reserve(result.vertexCount);
  result.rtVertices.reserve(result.vertexCount);

  for (uint32_t v = 0; v < result.vertexCount; v++) {
    Vertex vertex{};
    RayTracingVertex rtVertex{};

    glm::vec3 localPos =
        glm::vec3(positionBuffer[v * 3 + 0], positionBuffer[v * 3 + 1],
                  positionBuffer[v * 3 + 2]);
    glm::vec4 worldPos = worldTransform * glm::vec4(localPos, 1.0f);
    vertex.pos = glm::vec3(worldPos);
    rtVertex.position = glm::vec4(localPos, 1.0f);

    if (colorBuffer) {
      vertex.color = glm::vec3(colorBuffer[v * colorComponentCount + 0],
                               colorBuffer[v * colorComponentCount + 1],
                               colorBuffer[v * colorComponentCount + 2]);
    } else {
      vertex.color = glm::vec3(1.0f);
    }

    if (texCoordBuffer) {
      vertex.texCoord =
          glm::vec2(texCoordBuffer[v * 2 + 0], texCoordBuffer[v * 2 + 1]);
    } else {
      vertex.texCoord = glm::vec2(0.0f);
    }
    rtVertex.texCoord = vertex.texCoord;

    glm::vec3 localNormal = glm::vec3(0.0f, 0.0f, 1.0f);
    if (normalBuffer) {
      localNormal = glm::vec3(normalBuffer[v * 3 + 0], normalBuffer[v * 3 + 1],
                              normalBuffer[v * 3 + 2]);
    }

    vertex.normal = normalMatrix * localNormal;
    rtVertex.normal = glm::vec4(glm::normalize(localNormal), 0.0f);

    result.vertices.push_back(vertex);
    result.rtVertices.push_back(rtVertex);
  }

  if (primitive.indices > -1) {
    const tinygltf::Accessor &accessor = gltfModel.accessors[primitive.indices];
    const tinygltf::BufferView &bufferView =
        gltfModel.bufferViews[accessor.bufferView];
    const void *dataPtr =
        &gltfModel.buffers[bufferView.buffer]
             .data[accessor.byteOffset + bufferView.byteOffset];

    result.indexCount = static_cast<uint32_t>(accessor.count);
    result.indices.reserve(result.indexCount);

    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
      const uint32_t *buf = static_cast<const uint32_t *>(dataPtr);
      for (size_t i = 0; i < accessor.count; i++) {
        result.indices.push_back(buf[i]);
      }
      break;
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
      const uint16_t *buf = static_cast<const uint16_t *>(dataPtr);
      for (size_t i = 0; i < accessor.count; i++) {
        result.indices.push_back(buf[i]);
      }
      break;
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
      const uint8_t *buf = static_cast<const uint8_t *>(dataPtr);
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

void ObjectLoader::loadMesh(const tinygltf::Model &gltfModel,
                            const tinygltf::Mesh &gltfMesh, Model &model,
                            const glm::mat4 &worldTransform) {
  Mesh mesh;
  mesh.name = gltfMesh.name;

  glm::mat3 normalMatrix =
      glm::transpose(glm::inverse(glm::mat3(worldTransform)));

  // Process primitives in parallel
  size_t primCount = gltfMesh.primitives.size();
  std::vector<std::future<PrimitiveData>> futures;
  futures.reserve(primCount);

  for (const auto &primitive : gltfMesh.primitives) {
    futures.push_back(
        std::async(std::launch::async, &ObjectLoader::loadPrimitiveData, this,
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
    // KHR_materials_variants: per-primitive material mappings
    prim.variantMaterials.assign(model.variantNames.size(), -1);
    const auto &primExt = gltfMesh.primitives[pi].extensions;
    auto varIt = primExt.find("KHR_materials_variants");
    if (varIt != primExt.end() && varIt->second.Has("mappings")) {
      const tinygltf::Value &mappings = varIt->second.Get("mappings");
      for (size_t mi = 0; mi < mappings.ArrayLen(); ++mi) {
        const tinygltf::Value &mapping = mappings.Get(mi);
        if (!mapping.Has("material") || !mapping.Has("variants"))
          continue;
        const int32_t variantMaterial = mapping.Get("material").Get<int>();
        const tinygltf::Value &variantIndices = mapping.Get("variants");
        for (size_t v = 0; v < variantIndices.ArrayLen(); ++v) {
          const int32_t variantIndex = variantIndices.Get(v).Get<int>();
          if (variantIndex >= 0 &&
              variantIndex <
                  static_cast<int32_t>(prim.variantMaterials.size())) {
            prim.variantMaterials[variantIndex] = variantMaterial;
          }
        }
      }
    }

    prim.vertexCount = data.vertexCount;
    prim.indexCount = data.indexCount;

    // Merge collected data into model
    model.vertices.insert(model.vertices.end(), data.vertices.begin(),
                          data.vertices.end());
    model.rtVertices.insert(model.rtVertices.end(), data.rtVertices.begin(),
                            data.rtVertices.end());

    for (uint32_t idx : data.indices) {
      model.indices.push_back(idx + vertexOffset);
    }

    vertexOffset += data.vertexCount;
    indexOffset += data.indexCount;

    mesh.primitives.push_back(prim);
  }

  model.meshes.push_back(mesh);
}

glm::mat4 ObjectLoader::getNodeTransform(const tinygltf::Node &node) {
  glm::mat4 transform = glm::mat4(1.0f);

  if (node.matrix.size() == 16) {
    // Use matrix directly
    transform = glm::make_mat4(node.matrix.data());
  } else {
    // Build from TRS
    if (node.translation.size() == 3) {
      transform = glm::translate(transform, glm::vec3(node.translation[0],
                                                      node.translation[1],
                                                      node.translation[2]));
    }
    if (node.rotation.size() == 4) {
      glm::quat q(static_cast<float>(node.rotation[3]),
                  static_cast<float>(node.rotation[0]),
                  static_cast<float>(node.rotation[1]),
                  static_cast<float>(node.rotation[2]));
      transform *= glm::mat4_cast(q);
    }
    if (node.scale.size() == 3) {
      transform = glm::scale(
          transform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
    }
  }

  return transform;
}

void ObjectLoader::createModelBuffers(Model &model) {
  std::lock_guard<std::mutex> lock(vulkanMutex);

  if (model.vertices.empty() || model.indices.empty()) {
    std::cerr << "Cannot create buffers for empty model!" << std::endl;
    return;
  }

  // Vertex buffer
  VkDeviceSize vertexBufferSize = sizeof(Vertex) * model.vertices.size();
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  device->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       stagingBuffer, stagingBufferMemory);

  void *data;
  vkMapMemory(device->getDevice(), stagingBufferMemory, 0, vertexBufferSize, 0,
              &data);
  memcpy(data, model.vertices.data(), vertexBufferSize);
  vkUnmapMemory(device->getDevice(), stagingBufferMemory);

  device->createBuffer(
      vertexBufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, model.vertexBuffer,
      model.vertexBufferMemory);

  // Ray tracing vertex buffer (local positions/normals)
  VkDeviceSize rtVertexBufferSize =
      sizeof(RayTracingVertex) * model.rtVertices.size();
  if (rtVertexBufferSize > 0) {
    VkBuffer rtStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory rtStagingMemory = VK_NULL_HANDLE;
    device->createBuffer(rtVertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         rtStagingBuffer, rtStagingMemory);

    vkMapMemory(device->getDevice(), rtStagingMemory, 0, rtVertexBufferSize, 0,
                &data);
    memcpy(data, model.rtVertices.data(), rtVertexBufferSize);
    vkUnmapMemory(device->getDevice(), rtStagingMemory);

    device->createBuffer(
        rtVertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, model.rtVertexBuffer,
        model.rtVertexBufferMemory);

    device->copyBuffer(rtStagingBuffer, model.rtVertexBuffer,
                       rtVertexBufferSize);
    vkDestroyBuffer(device->getDevice(), rtStagingBuffer, nullptr);
    vkFreeMemory(device->getDevice(), rtStagingMemory, nullptr);
  }

  device->copyBuffer(stagingBuffer, model.vertexBuffer, vertexBufferSize);
  vkDestroyBuffer(device->getDevice(), stagingBuffer, nullptr);
  vkFreeMemory(device->getDevice(), stagingBufferMemory, nullptr);

  // Index buffer
  VkDeviceSize indexBufferSize = sizeof(uint32_t) * model.indices.size();

  device->createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       stagingBuffer, stagingBufferMemory);

  vkMapMemory(device->getDevice(), stagingBufferMemory, 0, indexBufferSize, 0,
              &data);
  memcpy(data, model.indices.data(), indexBufferSize);
  vkUnmapMemory(device->getDevice(), stagingBufferMemory);

  device->createBuffer(
      indexBufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, model.indexBuffer,
      model.indexBufferMemory);

  device->copyBuffer(stagingBuffer, model.indexBuffer, indexBufferSize);
  vkDestroyBuffer(device->getDevice(), stagingBuffer, nullptr);
  vkFreeMemory(device->getDevice(), stagingBufferMemory, nullptr);

  std::cout << "Created GPU buffers - Vertices: " << vertexBufferSize
            << " bytes, Indices: " << indexBufferSize << " bytes" << std::endl;
}

void ObjectLoader::destroyModel(Model &model) {
  std::lock_guard<std::mutex> lock(vulkanMutex);

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
  for (auto &texture : model.textures) {
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
