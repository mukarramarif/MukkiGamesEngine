#version 450

#define MAX_LIGHTS 4

// Debug modes - change this to test different visualizations
#define DEBUG_MODE 0
// 0 = Normal rendering (final lit result)
// 1 = Visualize normals
// 2 = Visualize world position
// 3 = Diffuse only (no specular)
// 4 = Specular only
// 5 = Light direction
// 6 = Albedo only (texture * color, no lighting)

struct Light {
    vec4 position;
    vec4 color;
    vec4 direction;
    vec4 attenuation;
};

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragWorldPos;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 normalMatrix;
    vec4 viewPos;
    Light lights[4];
    int numLights;
    float ambientStrength;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

vec3 calculateDirectionalLight(Light light, vec3 normal, vec3 viewDir, vec3 albedo) {
    vec3 lightDir = normalize(-light.direction.xyz);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * light.color.rgb * light.color.a;
    
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular = spec * light.color.rgb * light.color.a * 0.5;
    
    return (diffuse + specular) * albedo;
}

vec3 calculatePointLight(Light light, vec3 normal, vec3 viewDir, vec3 fragPos, vec3 albedo) {
    vec3 lightDir = normalize(light.position.xyz - fragPos);
    float distance = length(light.position.xyz - fragPos);
    
    float attenuation = 1.0 / (light.attenuation.x + 
                               light.attenuation.y * distance + 
                               light.attenuation.z * distance * distance);
    
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * light.color.rgb * light.color.a;
    
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular = spec * light.color.rgb * light.color.a * 0.5;
    
    return (diffuse + specular) * attenuation * albedo;
}

vec3 calculateSpotLight(Light light, vec3 normal, vec3 viewDir, vec3 fragPos, vec3 albedo) {
    vec3 lightDir = normalize(light.position.xyz - fragPos);
    
    float theta = dot(lightDir, normalize(-light.direction.xyz));
    float cutoff = light.direction.w;
    float outerCutoff = cutoff - 0.1;
    float epsilon = cutoff - outerCutoff;
    float intensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
    
    if (intensity > 0.0) {
        return calculatePointLight(light, normal, viewDir, fragPos, albedo) * intensity;
    }
    return vec3(0.0);
}

void main() {
    vec3 albedo = texture(texSampler, fragTexCoord).rgb * fragColor;
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(ubo.viewPos.xyz - fragWorldPos);

    // ============ DEBUG VISUALIZATIONS ============
    
    #if DEBUG_MODE == 1
        // Normals as colors: RGB = XYZ mapped from [-1,1] to [0,1]
        outColor = vec4(normal * 0.5 + 0.5, 1.0);
        return;
    #endif
    
    #if DEBUG_MODE == 2
        // World position (scaled for visibility)
        outColor = vec4(fract(fragWorldPos * 0.1), 1.0);
        return;
    #endif
    
    #if DEBUG_MODE == 5
        // Light direction to first light
        if (ubo.numLights > 0) {
            vec3 lightDir = normalize(ubo.lights[0].position.xyz - fragWorldPos);
            outColor = vec4(lightDir * 0.5 + 0.5, 1.0);
        } else {
            outColor = vec4(1.0, 0.0, 1.0, 1.0); // Magenta = no lights
        }
        return;
    #endif
    
    #if DEBUG_MODE == 6
        // Albedo only - no lighting
        outColor = vec4(albedo, 1.0);
        return;
    #endif

    // ============ LIGHTING CALCULATION ============
    
    vec3 ambient = ubo.ambientStrength * albedo;
    vec3 lighting = vec3(0.0);
    
    for (int i = 0; i < ubo.numLights && i < MAX_LIGHTS; i++) {
        int lightType = int(ubo.lights[i].attenuation.w);
        
        if (lightType == 0) {
            lighting += calculateDirectionalLight(ubo.lights[i], normal, viewDir, albedo);
        } else if (lightType == 1) {
            lighting += calculatePointLight(ubo.lights[i], normal, viewDir, fragWorldPos, albedo);
        } else if (lightType == 2) {
            lighting += calculateSpotLight(ubo.lights[i], normal, viewDir, fragWorldPos, albedo);
        }
    }
    
    #if DEBUG_MODE == 3
        // Diffuse only (approximate by removing specular term)
        outColor = vec4(ambient + lighting * 0.7, 1.0);
        return;
    #endif
    
    #if DEBUG_MODE == 4
        // Specular only
        outColor = vec4(lighting * 0.3, 1.0);
        return;
    #endif

    // Normal rendering
    vec3 result = ambient + lighting;
    result = result / (result + vec3(1.0)); // Tone mapping
    
    outColor = vec4(result, 1.0);
}