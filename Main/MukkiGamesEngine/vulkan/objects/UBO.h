#pragma once
#include <glm/glm.hpp>
#include "lights.h"

struct UniformBufferObject {
    glm::mat4 model{};
    glm::mat4 view{};
    glm::mat4 proj{};
    glm::mat4 normalMatrix{};         // inverse transpose of model matrix
    glm::mat4 lightSpaceMatrix{};     // directional light view-projection for shadow mapping
    glm::vec4 viewPos{};              // camera position
    GPULight lights[MAX_LIGHTS];
    int numLights{};
    float ambientStrength{};
    float padding[2]{};
    glm::vec4 pointShadowParams;
};


struct MaterialUBO {
    alignas(4) float metallicFactor;
    alignas(4) float roughnessFactor;
    alignas(4) float clearCoatFactor;
    alignas(4) float clearCoatRoughness;
	alignas(4) float baseColorR;
	alignas(4) float baseColorG;
	alignas(4) float baseColorB;
	alignas(4) float alpha;
	// KHR_texture_transform per slot: offsetX, offsetY, rotation, scaleX, scaleY
	alignas(4) float baseColorUvOx, baseColorUvOy, baseColorUvRot, baseColorUvSx, baseColorUvSy;
	alignas(4) float metallicRoughnessUvOx, metallicRoughnessUvOy, metallicRoughnessUvRot, metallicRoughnessUvSx, metallicRoughnessUvSy;
	alignas(4) float emissiveUvOx, emissiveUvOy, emissiveUvRot, emissiveUvSx, emissiveUvSy;
	// KHR_materials_iridescence
	alignas(4) float iridescenceFactor;
	alignas(4) float iridescenceIor;
	alignas(4) float iridescenceThicknessMin;
	alignas(4) float iridescenceThicknessMax;
	// KHR_materials_diffuse_transmission
	alignas(4) float diffuseTransmissionFactor;
	alignas(4) float diffuseTransmissionR, diffuseTransmissionG, diffuseTransmissionB;
	// KHR_materials_volume
	alignas(4) float attenuationR, attenuationG, attenuationB;
	alignas(4) float attenuationDistance;
	alignas(4) float thicknessFactor;
	// KHR_materials_transmission (glass)
	alignas(4) float transmissionFactor;
	alignas(4) float idxReflect;
};
