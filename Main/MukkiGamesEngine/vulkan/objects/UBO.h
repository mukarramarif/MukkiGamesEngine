#pragma once
#include <glm/glm.hpp>
#include "lights.h"

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 normalMatrix;    // inverse transpose of model matrix
    glm::vec4 viewPos;         // camera position
    GPULight lights[MAX_LIGHTS];
    int numLights;
    float ambientStrength;
    float padding[2];          // alignment to 16 bytes
};


struct MaterialUBO {
    alignas(4) float metallicFactor;
    alignas(4) float roughnessFactor;
    alignas(8) glm::vec2 padding;
};
