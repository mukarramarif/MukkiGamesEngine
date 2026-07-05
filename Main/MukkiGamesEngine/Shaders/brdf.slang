// Slang BRDF Shader - Cook-Torrance PBR with GGX microfacet model
// Directional light shadow mapping via R32 shadow map
// Matches the existing GLSL interface for drop-in replacement
// Inspo from Sasha Williams Slang for PBR Example: https://github.com/SaschaWillems/Vulkan/blob/master/shaders/slang/pbrbasic/pbr.slang

struct GPULight {
    float4 position;
    float4 color;
    float4 direction;
    float4 attenuation;
};

struct UniformBufferObject {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 normalMatrix;
    float4x4 lightSpaceMatrix;
    float4 viewPos;
    GPULight lights[4];
    int numLights;
    float ambientStrength;
    float2 padding;
};

struct MaterialUBO {
    float metallicFactor;
    float roughnessFactor;
    float2 padding;
};

[[vk::binding(0, 0)]] ConstantBuffer<UniformBufferObject> ubo;
[[vk::binding(1, 0)]] Sampler2D<float4> texSampler;
[[vk::binding(2, 0)]] ConstantBuffer<MaterialUBO> material;
[[vk::binding(3, 0)]] Sampler2D<float> shadowMap;

struct PSInput {
    [[vk::location(0)]] float3 fragColor : COLOR;
    [[vk::location(1)]] float2 fragTexCoord : TEXCOORD;
    [[vk::location(2)]] float3 fragNormal : NORMAL;
    [[vk::location(3)]] float3 fragWorldPos : POSITION;
    [[vk::location(4)]] float4 fragLightSpacePos : LIGHTSPACEPOS;
};

struct PSOutput {
    [[vk::location(0)]] float4 outColor : SV_Target;
};

// GGX/Trowbridge-Reitz NDF
float distributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (3.14159265 * denom * denom);
}

// Schlick-GGX geometry function
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

// Fresnel-Schlick
float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Percentage-Closer Filtering shadow calculation for a single directional light
float calculateShadow(float4 lightSpacePos) {
    // Perspective divide
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    // Transform from [-1,1] to [0,1]
    projCoords = projCoords * 0.5 + 0.5;

    // Outside the light frustum → no shadow
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 0.0;
    }

    float currentDepth = projCoords.z;
    float bias = 0.005;

    // 3x3 PCF
    float shadow = 0.0;
    float2 texelSize = float2(ubo.padding.x, ubo.padding.x);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float pcfDepth = shadowMap.Sample(projCoords.xy + float2(x, y) * texelSize);
            shadow += (currentDepth - bias) < pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    return shadow;
}

float3 calculateDirectionalLight(GPULight light, float3 normal, float3 viewDir, float3 albedo, float metallic, float roughness, float shadow) {
    float3 lightDir = normalize(-light.direction.xyz);
    float3 halfwayDir = normalize(lightDir + viewDir);

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    float NDF = distributionGGX(normal, halfwayDir, roughness);
    float G = geometrySmith(normal, viewDir, lightDir, roughness);
    float3 F = fresnelSchlick(max(dot(halfwayDir, viewDir), 0.0), F0);

    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - metallic;

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0) + 0.0001;
    float3 specular = numerator / denominator;

    float NdotL = max(dot(normal, lightDir), 0.0);
    float3 diffuse = kD * albedo / 3.14159265;

    return (diffuse + specular) * light.color.rgb * light.color.a * NdotL * (1.0 - shadow);
}

float3 calculatePointLight(GPULight light, float3 normal, float3 viewDir, float3 fragPos, float3 albedo, float metallic, float roughness) {
    float3 lightDir = normalize(light.position.xyz - fragPos);
    float distance = length(light.position.xyz - fragPos);
    float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * distance * distance);
    // Point lights don't use the directional shadow map, so shadow = 0.0
    return calculateDirectionalLight(light, normal, viewDir, albedo, metallic, roughness, 0.0) * attenuation;
}

float3 calculateSpotLight(GPULight light, float3 normal, float3 viewDir, float3 fragPos, float3 albedo, float metallic, float roughness) {
    float3 lightDir = normalize(light.position.xyz - fragPos);
    float theta = dot(lightDir, normalize(-light.direction.xyz));
    float cutoff = light.direction.w;
    float outerCutoff = cutoff - 0.1;
    float epsilon = cutoff - outerCutoff;
    float intensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
    if (intensity > 0.0) {
        return calculatePointLight(light, normal, viewDir, fragPos, albedo, metallic, roughness) * intensity;
    }
    return float3(0.0, 0.0, 0.0);
}

[shader("fragment")]
PSOutput main(PSInput input) {
    PSOutput output;

    float3 albedo = texSampler.Sample(input.fragTexCoord).rgb * input.fragColor;
    float3 normal = normalize(input.fragNormal);
    float3 viewDir = normalize(ubo.viewPos.xyz - input.fragWorldPos);

    // Material properties from UBO or constants
    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;

    // Compute shadow factor from directional light shadow map
    float shadow = calculateShadow(input.fragLightSpacePos);
    // float shadow = 0;
    float3 ambient = ubo.ambientStrength * albedo;
    float3 lighting = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < ubo.numLights && i < 4; i++) {
        int lightType = int(ubo.lights[i].attenuation.w);
        if (lightType == 0) {
            // Directional light: apply shadow factor
            lighting += calculateDirectionalLight(ubo.lights[i], normal, viewDir, albedo, metallic, roughness, shadow);
        } else if (lightType == 1) {
            lighting += calculatePointLight(ubo.lights[i], normal, viewDir, input.fragWorldPos, albedo, metallic, roughness);
        } else if (lightType == 2) {
            lighting += calculateSpotLight(ubo.lights[i], normal, viewDir, input.fragWorldPos, albedo, metallic, roughness);
        }
    }

    float3 result = ambient + lighting;
    result = result / (result + float3(1.0, 1.0, 1.0));
    output.outColor = float4(result, 1.0);
    return output;
}
