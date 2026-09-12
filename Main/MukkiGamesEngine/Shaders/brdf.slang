// Slang BRDF Shader - Cook-Torrance PBR with GGX microfacet model + Clear Coat
// Directional light shadow mapping via R32 shadow map
// Matches the existing GLSL interface for drop-in replacement
// Inspo from c: https://github.com/SaschaWillems/Vulkan/blob/master/shaders/slang/pbrbasic/pbr.slang

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
    float4 pointShadowParams;
};

struct MaterialUBO {
    float metallicFactor;
    float roughnessFactor;
    float clearCoatFactor;
    float clearCoatRoughness;
    // NOTE: all scalars on purpose - a float3 member would be std140-aligned
    // to 16 bytes and desync the field offsets from the C++ side.
    float baseColorR;
    float baseColorG;
    float baseColorB;
    float alpha;
    // KHR_texture_transform per slot: offsetX, offsetY, rotation, scaleX, scaleY
    float baseColorUvOx;
    float baseColorUvOy;
    float baseColorUvRot;
    float baseColorUvSx;
    float baseColorUvSy;
    float metallicRoughnessUvOx;
    float metallicRoughnessUvOy;
    float metallicRoughnessUvRot;
    float metallicRoughnessUvSx;
    float metallicRoughnessUvSy;
    float emissiveUvOx;
    float emissiveUvOy;
    float emissiveUvRot;
    float emissiveUvSx;
    float emissiveUvSy;
    // KHR_materials_iridescence
    float iridescenceFactor;
    float iridescenceIor;
    float iridescenceThicknessMin;
    float iridescenceThicknessMax;
    // KHR_materials_diffuse_transmission
    float diffuseTransmissionFactor;
    float diffuseTransmissionR;
    float diffuseTransmissionG;
    float diffuseTransmissionB;
    // KHR_materials_volume
    float attenuationR;
    float attenuationG;
    float attenuationB;
    float attenuationDistance;
    float thicknessFactor;
    // KHR_materials_transmission (glass)
    float transmissionFactor;
    float idxReflect;
};

[[vk::binding(0, 0)]] ConstantBuffer<UniformBufferObject> ubo;
[[vk::binding(1, 0)]] Sampler2D<float4> texSampler;
[[vk::binding(2, 0)]] ConstantBuffer<MaterialUBO> material;
[[vk::binding(3, 0)]] Sampler2D<float> shadowMap;
[[vk::binding(4, 0)]] SamplerCube<float> shadowCubeMap;

// ---- Ray-traced probe GI (mirrors Shaders/probeCommon.glsl) ----
static const int PROBE_IRRADIANCE_TEXELS = 8;
static const int PROBE_DEPTH_TEXELS = 16;
static const float PROBE_GI_STRENGTH = 1.0;  // dial for debugging
// Glass tuning: env reflection brightness + how much to soften the
// Beer-Lambert absorption (1.0 = physically correct, lower = brighter).
static const float GLASS_ENV_STRENGTH = 5.0;
static const float GLASS_ATTENUATION_SOFTEN = 0.5;

struct ProbeVolumeParams {
    float4 origin;      // xyz = grid min corner
    int4 probeCounts;   // xyz = probes per axis, w = total probe count
    float4 params;      // x = spacing, y = maxRayDistance, z = hysteresis, w = normalBias
    float4 atlas;       // x = tilesPerSide, y = irradianceTexelsPerProbe, z = depthTexelsPerProbe, w = raysPerProbe
};

[[vk::binding(5, 0)]] Sampler2D<float4> probeIrradiance;  // atlas, GENERAL layout
[[vk::binding(6, 0)]] Sampler2D<float2> probeDepth;       // atlas, GENERAL layout
[[vk::binding(7, 0)]] ConstantBuffer<ProbeVolumeParams> probeVolume;
[[vk::binding(8, 0)]] SamplerCube<float4> skyboxCube;     // glass env reflections

float2 probeOctEncode(float3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0) {
        float2 tmp = (1.0 - abs(n.yx)) * sign(n.xy);
        n.x = tmp.x;
        n.y = tmp.y;
    }
    return n.xy * 0.5 + 0.5;
}

float probeChebyshevWeight(float2 moments, float depth) {
    float mean = moments.x;
    float variance = max(moments.y - mean * mean, 0.0001);
    float d = depth - mean;
    float p = (d > 0.0) ? variance / (variance + d * d) : 1.0;
    return max(p * p * p, 0.05);
}

// Trilinear + octahedral + Chebyshev lookup of the probe volume.
// The atlases are sampled with a NEAREST clamp sampler at normalized
// texel centers, which is equivalent to the GLSL texelFetch path.
float3 sampleProbeGI(float3 pos, float3 normal) {
    int3 counts = probeVolume.probeCounts.xyz;
    if (counts.x < 2 || counts.y < 2 || counts.z < 2)
        return float3(0.0, 0.0, 0.0);

    float3 origin = probeVolume.origin.xyz;
    float spacing = probeVolume.params.x;
    float normalBias = probeVolume.params.w;
    float tilesPerSide = probeVolume.atlas.x;
    float irrAtlasSize = tilesPerSide * float(PROBE_IRRADIANCE_TEXELS);
    float depthAtlasSize = tilesPerSide * float(PROBE_DEPTH_TEXELS);

    float3 biasedPos = pos + normal * normalBias;
    float3 q = (biasedPos - origin) / spacing;
    float3 qc = clamp(q, float3(0.5, 0.5, 0.5),
                      float3(counts) - float3(0.5001, 0.5001, 0.5001));
    float3 basef = qc - float3(0.5, 0.5, 0.5);
    int3 base = int3(floor(basef));
    float3 frac = basef - float3(base);

    float3 result = float3(0.0, 0.0, 0.0);
    float totalWeight = 0.0;

    for (int dz = 0; dz <= 1; dz++)
    for (int dy = 0; dy <= 1; dy++)
    for (int dx = 0; dx <= 1; dx++) {
        int3 idx = base + int3(dx, dy, dz);
        int probeIndex = (idx.z * counts.y + idx.y) * counts.x + idx.x;

        float3 w = float3(1.0, 1.0, 1.0) - abs(float3(dx, dy, dz) - frac);
        float weight = w.x * w.y * w.z;
        if (weight <= 0.0)
            continue;

        float3 probePos = origin + (float3(idx) + float3(0.5, 0.5, 0.5)) * spacing;
        float3 toProbe = probePos - biasedPos;
        float distToProbe = length(toProbe);
        float3 dirToProbe = toProbe / max(distToProbe, 1e-5);

        float2 oct = probeOctEncode(dirToProbe);
        int2 tile = int2(probeIndex % int(tilesPerSide),
                         probeIndex / int(tilesPerSide));
        int2 irrTexel = int2(clamp(oct * float(PROBE_IRRADIANCE_TEXELS),
                                   float2(0.0, 0.0),
                                   float2(PROBE_IRRADIANCE_TEXELS - 1,
                                          PROBE_IRRADIANCE_TEXELS - 1)));
        int2 depthTexel = int2(clamp(oct * float(PROBE_DEPTH_TEXELS),
                                     float2(0.0, 0.0),
                                     float2(PROBE_DEPTH_TEXELS - 1,
                                            PROBE_DEPTH_TEXELS - 1)));
        int2 irrCoord = tile * PROBE_IRRADIANCE_TEXELS + irrTexel;
        int2 depthCoord = tile * PROBE_DEPTH_TEXELS + depthTexel;
        float2 irrUV = (float2(irrCoord) + float2(0.5, 0.5)) /
                       float2(irrAtlasSize, irrAtlasSize);
        float2 depthUV = (float2(depthCoord) + float2(0.5, 0.5)) /
                         float2(depthAtlasSize, depthAtlasSize);

        float3 irr = probeIrradiance.Sample(irrUV).rgb;
        float2 moments = probeDepth.Sample(depthUV).rg;
        float visibility = probeChebyshevWeight(moments, distToProbe);

        result += irr * (weight * visibility);
        totalWeight += weight * visibility;
    }
    return totalWeight > 0.0 ? result / totalWeight : float3(0.0, 0.0, 0.0);
}
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

// KHR_texture_transform: uv' = R * S * uv + O
float2 applyKhrTextureTransform(float2 uv, float ox, float oy, float rot, float sx, float sy) {
    float c = cos(rot);
    float s = sin(rot);
    return float2(c * sx * uv.x - s * sy * uv.y + ox,
                  s * sx * uv.x + c * sy * uv.y + oy);
}

// Thin-film iridescence (KHR_materials_iridescence), ported from rt.rgen
float3 iridescenceFresnel(float cosTheta, float filmIor, float thickness, float3 baseF0) {
    float c0 = clamp(cosTheta, 0.0, 1.0);
    float n0 = 1.0;                  // air
    float n1 = max(filmIor, 1.0);    // thin film

    // Substrate IOR per channel (dielectric F0=0.04 -> 1.5, metals from baseColor)
    float3 sqrtF0 = sqrt(clamp(baseF0, 0.0, 1.0));
    float3 n2 = (float3(1.0, 1.0, 1.0) + sqrtF0) /
                max(float3(1.0, 1.0, 1.0) - sqrtF0, float3(0.001, 0.001, 0.001));

    float sin0Sq = 1.0 - c0 * c0;
    float sin1Sq = (n0 * n0 / (n1 * n1)) * sin0Sq;
    float3 sin2Sq = (n1 * n1 / (n2 * n2)) * sin1Sq;
    float c1 = sqrt(max(1.0 - sin1Sq, 0.0));
    float3 c2 = sqrt(max(float3(1.0, 1.0, 1.0) - sin2Sq, float3(0.0, 0.0, 0.0)));

    // Amplitude reflection coefficients: s and p polarization
    float rs12 = (n0 * c0 - n1 * c1) / max(n0 * c0 + n1 * c1, 1e-5);
    float3 rs23 = (n1 * c1 - n2 * c2) / max(n1 * c1 + n2 * c2, float3(1e-5, 1e-5, 1e-5));
    float rp12 = (n1 * c0 - n0 * c1) / max(n1 * c0 + n0 * c1, 1e-5);
    float3 rp23 = (n2 * c1 - n1 * c2) / max(n2 * c1 + n1 * c2, float3(1e-5, 1e-5, 1e-5));

    float Rs12 = rs12 * rs12;
    float3 Rs23 = rs23 * rs23;
    float Rp12 = rp12 * rp12;
    float3 Rp23 = rp23 * rp23;

    // Round-trip phase per RGB channel (wavelengths in nm)
    float3 lambda = float3(650.0, 510.0, 475.0);
    float3 phase = (4.0 * 3.14159265 * n1 * thickness * c1) / lambda;

    float3 cosPhase = cos(phase);
    float3 Rs = (Rs12 + Rs23 + 2.0 * sqrt(Rs12 * Rs23) * cosPhase)
              / (float3(1.0, 1.0, 1.0) + Rs12 * Rs23 + 2.0 * sqrt(Rs12 * Rs23) * cosPhase);
    float3 Rp = (Rp12 + Rp23 + 2.0 * sqrt(Rp12 * Rp23) * cosPhase)
              / (float3(1.0, 1.0, 1.0) + Rp12 * Rp23 + 2.0 * sqrt(Rp12 * Rp23) * cosPhase);
    return 0.5 * (Rs + Rp);
}

float3 fresnelWithIridescence(float cosTheta, float3 F0) {
    if (material.iridescenceFactor > 0.001) {
        float filmThickness = lerp(material.iridescenceThicknessMin,
                                   material.iridescenceThicknessMax, 0.5);
        float3 Fbase = fresnelSchlick(cosTheta, F0);
        float3 Ffilm = iridescenceFresnel(cosTheta, material.iridescenceIor,
                                          filmThickness, F0);
        return lerp(Fbase, Ffilm, material.iridescenceFactor);
    }
    return fresnelSchlick(cosTheta, F0);
}

// Percentage-Closer Filtering shadow calculation for a single directional light
float calculateShadow(float4 lightSpacePos) {
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 0.0;
    }

    float currentDepth = projCoords.z;
    float bias = 0.005;

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
// Cube map shadows for the point light (linear depth)
float calculatePointLightShadow(float3 lightPos, float3 fragPos) {
    if (ubo.pointShadowParams.w <= 0.0) return 0.0;   // no shadowed point light

    float3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight) * ubo.pointShadowParams.w;  // linear, [0,1]
    float bias = 0.02;                                                    // tune for acne
    float closestDepth = shadowCubeMap.Sample(fragToLight);               // linear from Part A
    return (currentDepth - bias) < closestDepth ? 1.0 : 0.0;
}

// Clear coat helpers
static const float CLEAR_COAT_F0 = 0.04;
float3 fresnelSchlickClearCoat(float cosTheta) {
    return CLEAR_COAT_F0 + (1.0 - CLEAR_COAT_F0) * pow(1.0 - cosTheta, 5.0);
}

float3 clearCoatBRDF(float3 normal, float3 viewDir, float3 lightDir, float roughness) {
    float3 H = normalize(lightDir + viewDir);
    float NDF = distributionGGX(normal, H, roughness);
    float G = geometrySmith(normal, viewDir, lightDir, roughness);
    float3 F = fresnelSchlickClearCoat(max(dot(H, viewDir), 0.0));
    float3 num = NDF * G * F;
    float denom = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0) + 0.0001;
    return num / denom;
}

float V_Kelemen(float LoH){
    return 0.25/(LoH*LoH);
}

float3 calculateDirectionalLight(GPULight light, float3 normal, float3 viewDir, float3 albedo, float metallic, float roughness, float shadow) {
    float3 lightDir = normalize(-light.direction.xyz);
    float3 halfwayDir = normalize(lightDir + viewDir);

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    float NDF = distributionGGX(normal, halfwayDir, roughness);
    float G = geometrySmith(normal, viewDir, lightDir, roughness);

    float3 F = fresnelWithIridescence(max(dot(halfwayDir, viewDir), 0.0), F0);

    float3 kS = F;
    // KHR_materials_diffuse_transmission: the diffuse lobe is split between
    // reflection and transmission. KHR_materials_transmission (glass) has no
    // diffuse response at all.
    float3 kD = (float3(1.0, 1.0, 1.0) - kS) * (1.0 - metallic) *
                (1.0 - material.diffuseTransmissionFactor) *
                (1.0 - material.transmissionFactor);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0) + 0.0001;
    float3 specular = numerator / denominator;

    float NdotL = max(dot(normal, lightDir), 0.0);
    float3 diffuse = kD * albedo / 3.14159265;

    return (diffuse + specular) * light.color.rgb * light.color.a * NdotL * (1.0 - shadow);
}
float3 calculateLight(GPULight light, float3 lightDir, float3 normal, float3 viewDir,
                      float3 albedo, float metallic, float roughness, float shadow) {
    float3 halfwayDir = normalize(lightDir + viewDir);
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    float NDF = distributionGGX(normal, halfwayDir, roughness);
    float G = geometrySmith(normal, viewDir, lightDir, roughness);

    float3 F = fresnelWithIridescence(max(dot(halfwayDir, viewDir), 0.0), F0);

    float3 kS = F;
    // KHR_materials_diffuse_transmission: the diffuse lobe is split between
    // reflection and transmission. KHR_materials_transmission (glass) has no
    // diffuse response at all.
    float3 kD = (float3(1.0, 1.0, 1.0) - kS) * (1.0 - metallic) *
                (1.0 - material.diffuseTransmissionFactor) *
                (1.0 - material.transmissionFactor);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0) + 0.0001;
    float3 specular = numerator / denominator;

    float NdotL = max(dot(normal, lightDir), 0.0);
    float3 diffuse = kD * albedo / 3.14159265;

    return (diffuse + specular) * light.color.rgb * light.color.a * NdotL * (1.0 - shadow);
}
float3 calculatePointLight(GPULight light, float3 normal, float3 viewDir, float3 fragPos, float3 albedo, float metallic, float roughness, float shadow) {
    float3 lightDir = normalize(light.position.xyz - fragPos);
    float distance = length(light.position.xyz - fragPos);
    float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * distance * distance);
    return calculateLight(light, lightDir, normal, viewDir, albedo, metallic, roughness, shadow) * attenuation;
}

float3 calculateSpotLight(GPULight light, float3 normal, float3 viewDir, float3 fragPos, float3 albedo, float metallic, float roughness) {
    float3 lightDir = normalize(light.position.xyz - fragPos);
    float theta = dot(lightDir, normalize(-light.direction.xyz));
    float cutoff = light.direction.w;
    float outerCutoff = cutoff - 0.1;
    float epsilon = cutoff - outerCutoff;
    float intensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
    if (intensity > 0.0) {
        return calculatePointLight(light, normal, viewDir, fragPos, albedo, metallic, roughness, 0.0) * intensity;
    }
    return float3(0.0, 0.0, 0.0);
}

[shader("fragment")]
PSOutput main(PSInput input) {
    PSOutput output;

    // KHR_texture_transform on the base color slot
    float2 baseUv = applyKhrTextureTransform(input.fragTexCoord,
        material.baseColorUvOx, material.baseColorUvOy, material.baseColorUvRot,
        material.baseColorUvSx, material.baseColorUvSy);
    float4 texSample = texSampler.Sample(baseUv);
    float3 baseColorFactor = float3(material.baseColorR, material.baseColorG, material.baseColorB);
    float3 albedo = texSample.rgb * baseColorFactor * input.fragColor;
    float3 normal = normalize(input.fragNormal);
    float3 viewDir = normalize(ubo.viewPos.xyz - input.fragWorldPos);

    // Glass (KHR_materials_transmission): fresnel-weighted alpha - grazing
    // angles reflect (opaque-ish edges), head-on transmits (transparent).
    float transmission = material.transmissionFactor;
    float alpha = texSample.a * material.alpha;
    float3 gi = sampleProbeGI(input.fragWorldPos, normal) * PROBE_GI_STRENGTH;
    float3 glassColor = float3(0.0, 0.0, 0.0);
    if (transmission > 0.0) {
        float cosTheta = max(dot(normal, viewDir), 0.0);
        float iorF0 = (material.idxReflect - 1.0) / (material.idxReflect + 1.0);
        iorF0 *= iorF0;
        float F = iorF0 + (1.0 - iorF0) * pow(1.0 - cosTheta, 5.0);
        alpha = lerp(alpha, 1.0 - transmission * (1.0 - F), transmission);

        // Glass has no diffuse; its color comes from the environment seen
        // through/off the surface. Cheap approximation: skybox sampled along
        // the reflection direction, absorbed by the volume (Beer-Lambert).
        // The optical depth is softened: physically correct absorption reads
        // nearly black under dim studio lighting.
        float3 reflDir = reflect(-viewDir, normal);
        float3 env = skyboxCube.Sample(reflDir).rgb;
        // The studio skybox has large black regions; a dark reflection would
        // otherwise turn grazing surfaces into opaque black patches. Floor
        // the environment with the probe field + a small constant.
        env = max(env, gi * 0.5 / 3.14159265 + float3(0.02, 0.02, 0.02));
        float thickness = max(material.thicknessFactor, 0.0);
        float3 atten = pow(float3(material.attenuationR, material.attenuationG,
                                  material.attenuationB),
                           GLASS_ATTENUATION_SOFTEN * thickness /
                           max(material.attenuationDistance, 1e-3));
        glassColor = transmission * env * atten * GLASS_ENV_STRENGTH;

        // Don't let dark reflections drive the glass fully opaque: cap the
        // opacity by the brightness of the environment behind it.
        float envLum = max(max(env.r, env.g), env.b);
        alpha = min(alpha, clamp(envLum * 3.0, 0.08, 1.0));
    }

    float metallic = material.metallicFactor;
    // Floor the roughness: roughness 0 collapses the GGX distribution to a
    // zero-width spike, killing specular highlights entirely (glass depends
    // on them for its look).
    float roughness = max(material.roughnessFactor, 0.02);

    float shadow = calculateShadow(input.fragLightSpacePos);
    // Irradiance field stores cosine-weighted incoming radiance; the diffuse
    // response scales it by albedo / pi.
    float3 indirect = ubo.ambientStrength * albedo + gi * albedo / 3.14159265;

    // KHR_materials_diffuse_transmission: the diffuse lobe is split between
    // reflection and transmission, so the reflected side scales by (1 - dt).
    // Glass (transmission) has no diffuse response at all.
    float dt = material.diffuseTransmissionFactor;
    float3 ambient = indirect * (1.0 - dt) * (1.0 - transmission);

    // Transmitted side: approximate the light coming through the surface
    // (the raster path has no behind-scene buffer to sample). Beer-Lambert
    // absorption through the volume (KHR_materials_volume).
    float thickness = max(material.thicknessFactor, 0.0);
    float3 atten = pow(float3(material.attenuationR, material.attenuationG, material.attenuationB),
                       thickness / max(material.attenuationDistance, 1e-3));
    float3 transmitted = indirect * atten *
                         float3(material.diffuseTransmissionR,
                                material.diffuseTransmissionG,
                                material.diffuseTransmissionB);
    ambient += dt * (1.0 - transmission) * transmitted;
    float3 lighting = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < ubo.numLights && i < 4; i++) {
        int lightType = int(ubo.lights[i].attenuation.w);
        if (lightType == 0) {
            // Glass is rendered as an opaque caster into the shadow map, so
            // its interior faces pick up self-shadow patches that read as
            // random black artifacts once the surface is transparent.
            // Transmissive materials skip shadows (matches the RT glass path).
            float dirShadow = (transmission > 0.0) ? 0.0 : shadow;
            lighting += calculateDirectionalLight(ubo.lights[i], normal, viewDir, albedo, metallic, roughness, dirShadow);
        } else if (lightType == 1) {
            float pointShadow = calculatePointLightShadow(ubo.lights[i].position.xyz,
                                                          input.fragWorldPos);
            if (transmission > 0.0) pointShadow = 0.0;
            lighting += calculatePointLight(ubo.lights[i], normal, viewDir, input.fragWorldPos,
                                            albedo, metallic, roughness, pointShadow);
        } else if (lightType == 2) {
            lighting += calculateSpotLight(ubo.lights[i], normal, viewDir, input.fragWorldPos, albedo, metallic, roughness);
        }
    }

    float3 result = ambient + lighting + glassColor;

    // Clear coat layering (on top of base BRDF). Skipped for glass: the
    // coat fresnel zeroes the result at grazing angles, which combined with
    // fresnel alpha made opaque black patches on transmissive surfaces.
    if (material.clearCoatFactor > 0.0 && transmission <= 0.0) {
        float ccRoughness = material.clearCoatRoughness;
        float3 ccSpec = 0.0;
        [unroll] for (int i = 0; i < ubo.numLights && i < 4; i++) {
            int lightType = int(ubo.lights[i].attenuation.w);
            float3 L;
            float NdotL;
            float atten = 1.0;
            if (lightType == 0) {
                L = normalize(-ubo.lights[i].direction.xyz);
                NdotL = max(dot(normal, L), 0.0);
            } else {
                L = normalize(ubo.lights[i].position.xyz - input.fragWorldPos);
                float dist = length(ubo.lights[i].position.xyz - input.fragWorldPos);
                atten = 1.0 / (ubo.lights[i].attenuation.x + ubo.lights[i].attenuation.y * dist + ubo.lights[i].attenuation.z * dist * dist);
                NdotL = max(dot(normal, L), 0.0);
            }
            float3 lightColor = atten * ubo.lights[i].color.rgb * ubo.lights[i].color.a;
            ccSpec += clearCoatBRDF(normal, viewDir, L, ccRoughness) * lightColor * NdotL;
        }
        float3 ccFresnel = fresnelSchlickClearCoat(max(dot(normal, viewDir), 0.0));
        result = result * (1.0 - ccFresnel * material.clearCoatFactor) + ccSpec * material.clearCoatFactor;
    }

    // Reinhard tone mapping
    result = result / (result + float3(1.0, 1.0, 1.0));
    output.outColor = float4(result, alpha);
    return output;
}
