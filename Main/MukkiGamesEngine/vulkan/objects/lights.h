#pragma once
#include <glm/glm.hpp>

constexpr int MAX_LIGHTS = 4;

enum class LightType : int {
    Directional = 0,
    Point = 1,
    Spot = 2
};

struct GPULight {
    glm::vec4 position;      // xyz = position, w = unused
    glm::vec4 color;         // rgb = color, a = intensity
    glm::vec4 direction;     // xyz = direction, w = spotlight cutoff
    glm::vec4 attenuation;   // x = constant, y = linear, z = quadratic, w = type
};

struct Light {
    LightType type = LightType::Directional;
    glm::vec3 position = glm::vec3(0.0f, 5.0f, 0.0f);
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;

    // Attenuation (for point/spot lights)
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;

    // Spotlight
    float cutoffAngle = 12.5f;  // degrees

    bool enabled = true;

    GPULight toGPU() const {
        GPULight gpu;
        gpu.position = glm::vec4(position, 1.0f);
        gpu.color = glm::vec4(color, intensity);
        gpu.direction = glm::vec4(glm::normalize(direction), glm::cos(glm::radians(cutoffAngle)));
        gpu.attenuation = glm::vec4(constant, linear, quadratic, static_cast<float>(type));
        return gpu;
    }
};