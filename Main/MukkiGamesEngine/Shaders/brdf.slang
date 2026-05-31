// Slang BRDF Shader - Cook-Torrance PBR with GGX microfacet model
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
    float4 viewPos;
    GPULight lights[4];
    int numLights;
    float ambientStrength;
    float padding[2];
};

[[vk::binding(0, 0)]] ConstantBuffer<UniformBufferObject> ubo;

[[vk::binding(1, 0)]] Sampler2D<float4> texSampler;

struct PSInput {
    [[vk::location(0)]] float3 fragColor : COLOR;
    [[vk::location(1)]] float2 fragTexCoord : TEXCOORD;
    [[vk::location(2)]] float3 fragNormal : NORMAL;
    [[vk::location(3)]] float3 fragWorldPos : POSITION;
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

float3 calculateDirectionalLight(GPULight light, float3 normal, float3 viewDir, float3 albedo, float metallic, float roughness) {
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

    return (diffuse + specular) * light.color.rgb * light.color.a * NdotL;
}

float3 calculatePointLight(GPULight light, float3 normal, float3 viewDir, float3 fragPos, float3 albedo, float metallic, float roughness) {
    float3 lightDir = normalize(light.position.xyz - fragPos);
    float distance = length(light.position.xyz - fragPos);
    float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * distance * distance);
    return calculateDirectionalLight(light, normal, viewDir, albedo, metallic, roughness) * attenuation;
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
    float metallic = 0.5;
    float roughness = 0.3;

    float3 ambient = ubo.ambientStrength * albedo;
    float3 lighting = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < ubo.numLights && i < 4; i++) {
        int lightType = int(ubo.lights[i].attenuation.w);
        if (lightType == 0) {
            lighting += calculateDirectionalLight(ubo.lights[i], normal, viewDir, albedo, metallic, roughness);
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
