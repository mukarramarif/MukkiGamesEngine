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

static const int MAX_LIGHTS = 8;
static const int MAX_POINT_SHADOWS = 4;

struct UniformBufferObject {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 normalMatrix;
    float4x4 lightSpaceMatrix;
    float4 viewPos;
    GPULight lights[MAX_LIGHTS];
    int numLights;
    float ambientStrength;
    float2 padding;
    // Cube-shadow data, packed in shadow-slot order. xyz = position, w = 1/far.
    float4 pointShadowParams[MAX_POINT_SHADOWS];
    // The same point lights duplicated for static (unrolled) indexing.
    GPULight pointLights[MAX_POINT_SHADOWS];
    int numPointLights;
    float3 padPointLights;
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
    // KHR_materials_emissive (rt.rgen parity). The CPU zeroes these when no
    // emissive texture is bound, so the factor doubles as the enable flag.
    float emissiveR;
    float emissiveG;
    float emissiveB;
};

[[vk::binding(0, 0)]] ConstantBuffer<UniformBufferObject> ubo;
[[vk::binding(1, 0)]] Sampler2D<float4> texSampler;
[[vk::binding(2, 0)]] ConstantBuffer<MaterialUBO> material;
[[vk::binding(3, 0)]] Sampler2D<float> shadowMap;
[[vk::binding(4, 0)]] SamplerCube<float> shadowCubeMaps[MAX_POINT_SHADOWS];

// ---- Ray-traced probe GI (mirrors Shaders/probeCommon.glsl) ----
static const int PROBE_IRRADIANCE_TEXELS = 8;
// Must match PROBE_DEPTH_TEXELS in probeCommon.glsl: 256 rays / 8x8 = ~4
// samples per depth texel, so the moments have real variance.
static const int PROBE_DEPTH_TEXELS = 8;
// GI strength is a live knob now: probeVolume.debug.y (default 1.0).
// Glass tuning: how much to soften the Beer-Lambert absorption
// (1.0 = physically correct, lower = brighter).
static const float GLASS_ATTENUATION_SOFTEN = 0.5;

struct ProbeVolumeParams {
    float4 origin;      // xyz = grid min corner
    int4 probeCounts;   // xyz = probes per axis, w = total probe count
    float4 params;      // x = spacing, y = maxRayDistance, z = hysteresis, w = normalBias
    float4 atlas;       // x = tilesPerSide, y = irradianceTexelsPerProbe, z = depthTexelsPerProbe, w = raysPerProbe
    float4 debug;       // x = debug mode (0 off, 1 GI only, 2 heatmap, 3 probe cells), y = GI strength
    float4 shadow;      // x = probe shadow strength, y = penumbra softening, z = BRDF tap count (1 = legacy)
};

// std430 mirror of ProbeData (vulkan/Resources/ProbeVolume.h). Relocated
// probes must be sampled at their ACTUAL positions - the stored octahedral
// texel directions and depth moments are measured from the relocated origin.
struct ProbeData {
    float4 pos;          // xyz = probe world position
    float4 rotationSeed; // xy = ray rotation seed, z = probe quality
                         // (1 = healthy, 0 = stuck inside geometry)
};

[[vk::binding(5, 0)]] Sampler2D<float4> probeIrradiance;  // atlas, GENERAL layout
[[vk::binding(6, 0)]] Sampler2D<float2> probeDepth;       // atlas, GENERAL layout
[[vk::binding(7, 0)]] ConstantBuffer<ProbeVolumeParams> probeVolume;
[[vk::binding(8, 0)]] SamplerCube<float4> skyboxCube;     // unused by shading (probes capture the skybox); kept for the binding
[[vk::binding(9, 0)]] StructuredBuffer<ProbeData> probePositions;
[[vk::binding(10, 0)]] Sampler2D<float4> emissiveSampler; // KHR_materials_emissive (rt.rgen parity)

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
    // Relative variance floor: with only a few samples per depth texel the
    // raw variance can be ~0, which makes the test reject every query point
    // that is not exactly at the stored hit distance (flat nearest-probe
    // blotches). Flooring relative to the mean keeps the falloff smooth
    // without the old constant 0.05 visibility floor that leaked light.
    float variance = max(moments.y - mean * mean, 0.05 * mean * mean + 0.0001);
    float d = depth - mean;
    float p = (d > 0.0) ? variance / (variance + d * d) : 1.0;
    return p * p * p;
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

    // Fallback: prefer the nearest probe that is IN FRONT of the surface
    // (backface > 0.5); only use any probe if none is in front. Prevents
    // the fallback from returning an occluded probe's garbage.
    float bestFrontTriW = -1.0;
    float3 bestFrontIrradiance = float3(0.0, 0.0, 0.0);
    float bestAnyTriW = -1.0;
    float3 bestAnyIrradiance = float3(0.0, 0.0, 0.0);

    for (int dz = 0; dz <= 1; dz++)
    for (int dy = 0; dy <= 1; dy++)
    for (int dx = 0; dx <= 1; dx++) {
        int3 idx = base + int3(dx, dy, dz);
        int probeIndex = (idx.z * counts.y + idx.y) * counts.x + idx.x;

        float3 w = float3(1.0, 1.0, 1.0) - abs(float3(dx, dy, dz) - frac);
        float triW = w.x * w.y * w.z;
        if (triW <= 0.0)
            continue;

        // Relocated probes live in the SSBO; the grid formula would index
        // the wrong octahedral texel and break the Chebyshev depth test.
        float3 probePos = probePositions[uint(probeIndex)].pos.xyz;
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

        // Backface rejection: probes BEHIND the surface (normal points away
        // from them) leak light through walls - weight them down smoothly so
        // adjacent triangles don't pop between 0 and full weight.
        float backface = smoothstep(0.0, 0.2, dot(normal, dirToProbe));

        // Per-probe quality (rotationSeed.z): probes stuck inside geometry
        // store garbage radiance; downweight them so it can't leak into
        // neighboring cells.
        float quality = probePositions[uint(probeIndex)].rotationSeed.z;

        // Proximity downweight: probes hugging the surface store the
        // surface's OWN radiance in their toward-surface texels - sampling
        // them back self-illuminates the surface (localized blotch at
        // relocated probes). Fade close probes out; the other probes in
        // the cell carry the shading.
        float proxWeight = smoothstep(0.0, spacing * 0.35, distToProbe);

        float weight = triW * visibility * backface * (quality * quality) * proxWeight;
        result += irr * weight;
        totalWeight += weight;

        if (backface > 0.5) {
            if (triW > bestFrontTriW) {
                bestFrontTriW = triW;
                bestFrontIrradiance = irr;
            }
        } else if (triW > bestAnyTriW) {
            bestAnyTriW = triW;
            bestAnyIrradiance = irr;
        }
    }
    if (totalWeight > 1e-5)
        return result / totalWeight;
    if (bestFrontTriW >= 0.0)
        return bestFrontIrradiance;
    return bestAnyTriW >= 0.0 ? bestAnyIrradiance : float3(0.0, 0.0, 0.0);
}

// Deterministic per-pixel rotation seed so tap patterns decorrelate across
// fragments (avoids screen-aligned banding from the fibonacci kernel).
float probeHashRot(float3 p) {
    return frac(sin(dot(p, float3(12.9898, 78.233, 37.719))) * 43758.5453);
}

// Cosine-weighted hemisphere sample (fibonacci + golden angle). With this
// distribution the pdf cancels in the Monte Carlo estimator, so outgoing
// diffuse radiance is just albedo * mean tap radiance.
float3 probeCosineHemisphere(float3 n, int i, int taps, float rot) {
    float3 helper = abs(n.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 t = normalize(cross(helper, n));
    float3 b = cross(n, t);
    float u = (float(i) + 0.5) / float(taps);
    float phi = 2.399963 * float(i) + rot;
    float c = sqrt(u);
    float s = sqrt(max(1.0 - u, 0.0));
    return normalize(n * c + (t * cos(phi) + b * sin(phi)) * s);
}

// Sample inside a cone around 'axis' (uniform disk, mapped onto the sphere).
// coneSin controls the half-angle: small = sharp reflections, large = blurry.
float3 probeConeSample(float3 axis, int i, int taps, float coneSin, float rot) {
    float3 helper = abs(axis.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 t = normalize(cross(helper, axis));
    float3 b = cross(axis, t);
    float u = (float(i) + 0.5) / float(taps);
    float phi = 2.399963 * float(i) + rot;
    float r = coneSin * sqrt(u);
    return normalize(axis * sqrt(max(1.0 - r * r, 0.0)) +
                     (t * cos(phi) + b * sin(phi)) * r);
}

// Trilinear + octahedral + Chebyshev radiance fetch along an ARBITRARY world
// direction (used by the BRDF taps). Returns (radiance, totalWeight).
// Radiance is read from the texel for 'dir'; visibility uses the depth
// moments along the line of sight to each probe (the only direction the
// Chebyshev distance test is valid for).
float4 sampleProbeDirectional(float3 pos, float3 normal, float3 dir) {
    int3 counts = probeVolume.probeCounts.xyz;
    if (counts.x < 2 || counts.y < 2 || counts.z < 2)
        return float4(0.0, 0.0, 0.0, 0.0);

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
        float triW = w.x * w.y * w.z;
        if (triW <= 0.0)
            continue;

        float3 probePos = probePositions[uint(probeIndex)].pos.xyz;
        float3 toProbe = probePos - biasedPos;
        float distToProbe = length(toProbe);
        float3 dirToProbe = toProbe / max(distToProbe, 1e-5);

        int2 tile = int2(probeIndex % int(tilesPerSide),
                         probeIndex / int(tilesPerSide));

        // Radiance for the queried direction.
        float2 oct = probeOctEncode(dir);
        int2 irrTexel = int2(clamp(oct * float(PROBE_IRRADIANCE_TEXELS),
                                   float2(0.0, 0.0),
                                   float2(PROBE_IRRADIANCE_TEXELS - 1,
                                          PROBE_IRRADIANCE_TEXELS - 1)));
        int2 irrCoord = tile * PROBE_IRRADIANCE_TEXELS + irrTexel;
        float2 irrUV = (float2(irrCoord) + float2(0.5, 0.5)) /
                       float2(irrAtlasSize, irrAtlasSize);
        float3 irr = probeIrradiance.Sample(irrUV).rgb;

        // Visibility along the line of sight to the probe.
        float2 losOct = probeOctEncode(dirToProbe);
        int2 depthTexel = int2(clamp(losOct * float(PROBE_DEPTH_TEXELS),
                                     float2(0.0, 0.0),
                                     float2(PROBE_DEPTH_TEXELS - 1,
                                            PROBE_DEPTH_TEXELS - 1)));
        int2 depthCoord = tile * PROBE_DEPTH_TEXELS + depthTexel;
        float2 depthUV = (float2(depthCoord) + float2(0.5, 0.5)) /
                         float2(depthAtlasSize, depthAtlasSize);
        float2 moments = probeDepth.Sample(depthUV).rg;
        float visibility = probeChebyshevWeight(moments, distToProbe);

        float backface = smoothstep(0.0, 0.2, dot(normal, dirToProbe));

        // Per-probe quality: garbage probes (stuck in geometry) must not
        // leak into the BRDF taps either. Also fade probes hugging the
        // surface (proximity) so a relocated probe can't feed the surface
        // its own radiance back.
        float quality = probePositions[uint(probeIndex)].rotationSeed.z;
        float proxWeight = smoothstep(0.0, spacing * 0.35, distToProbe);
        float weight = triW * visibility * backface * (quality * quality) * proxWeight;
        result += irr * weight;
        totalWeight += weight;
    }
    return float4(result, totalWeight);
}

// BRDF diffuse term: cosine-weighted hemisphere taps through the probe field.
// Returns mean tap radiance (caller multiplies by albedo; the cosine-weighted
// pdf cancels in the estimator). taps == 1 falls back to the legacy
// single-direction irradiance lookup.
float3 sampleProbeDiffuse(float3 pos, float3 normal, int taps) {
    if (taps <= 1)
        return sampleProbeGI(pos, normal);

    float rot = probeHashRot(pos) * 6.2831853;
    float3 sum = float3(0.0, 0.0, 0.0);
    float wsum = 0.0;
    for (int i = 0; i < taps; i++) {
        float3 dir = probeCosineHemisphere(normal, i, taps, rot);
        float4 tap = sampleProbeDirectional(pos, normal, dir);
        sum += tap.rgb;
        wsum += tap.w;
    }
    return wsum > 1e-4 ? sum / max(wsum, 1e-4) : sampleProbeGI(pos, normal);
}

// BRDF specular term: cone taps around the reflection direction, spread by
// roughness (no roughness mips, so the cone average substitutes for the
// prefilter). Returns mean reflection radiance; the caller modulates with
// Fresnel. taps == 1 samples the mirror direction directly.
float3 sampleProbeSpecular(float3 pos, float3 normal, float3 viewDir,
                           float roughness, int taps) {
    float3 R = reflect(-viewDir, normal);
    float coneSin = lerp(0.03, 0.9, clamp(roughness, 0.0, 1.0));
    float rot = probeHashRot(pos + R * 3.0) * 6.2831853;

    float3 sum = float3(0.0, 0.0, 0.0);
    float wsum = 0.0;
    for (int i = 0; i < taps; i++) {
        float3 dir = probeConeSample(R, i, taps, coneSin, rot);
        float4 tap = sampleProbeDirectional(pos, normal, dir);
        sum += tap.rgb;
        wsum += tap.w;
    }
    return wsum > 1e-4 ? sum / max(wsum, 1e-4)
                       : sampleProbeGI(pos, normal) / 3.14159265;
}

// Probe-based soft shadow query: trilinearly blends the per-direction sun
// visibility stored in the irradiance atlas alpha channel, sampled along the
// direction toward the light. Occluders soften through the blend, giving
// contact-soft shadows without a shadow ray.
float sampleProbeShadow(float3 pos, float3 dirToLight, float3 normal) {
    int3 counts = probeVolume.probeCounts.xyz;
    if (counts.x < 2 || counts.y < 2 || counts.z < 2)
        return 1.0;

    float3 origin = probeVolume.origin.xyz;
    float spacing = probeVolume.params.x;
    float tilesPerSide = probeVolume.atlas.x;
    float irrAtlasSize = tilesPerSide * float(PROBE_IRRADIANCE_TEXELS);

    float3 q = (pos - origin) / spacing;
    float3 qc = clamp(q, float3(0.5, 0.5, 0.5),
                      float3(counts) - float3(0.5001, 0.5001, 0.5001));
    float3 basef = qc - float3(0.5, 0.5, 0.5);
    int3 base = int3(floor(basef));
    float3 frac = basef - float3(base);

    float result = 0.0;
    float totalWeight = 0.0;

    for (int dz = 0; dz <= 1; dz++)
    for (int dy = 0; dy <= 1; dy++)
    for (int dx = 0; dx <= 1; dx++) {
        int3 idx = base + int3(dx, dy, dz);
        int probeIndex = (idx.z * counts.y + idx.y) * counts.x + idx.x;

        float3 w = float3(1.0, 1.0, 1.0) - abs(float3(dx, dy, dz) - frac);
        float triW = w.x * w.y * w.z;
        if (triW <= 0.0)
            continue;

        // A probe on the far side of a wall is (correctly) shadowed at ITS
        // location, but a lit shading point in front of the wall must not
        // sample it: weight by the same backface/quality terms as the GI
        // path so shadowed probes can't paint dark bands on lit surfaces.
        float3 probePos = probePositions[uint(probeIndex)].pos.xyz;
        float3 toProbe = probePos - pos;
        float distToProbe = length(toProbe);
        float3 dirToProbe = toProbe / max(distToProbe, 1e-5);
        float backface = smoothstep(0.0, 0.2, dot(normal, dirToProbe));
        float quality = probePositions[uint(probeIndex)].rotationSeed.z;

        float2 oct = probeOctEncode(dirToLight);
        int2 tile = int2(probeIndex % int(tilesPerSide),
                         probeIndex / int(tilesPerSide));
        int2 irrTexel = int2(clamp(oct * float(PROBE_IRRADIANCE_TEXELS),
                                   float2(0.0, 0.0),
                                   float2(PROBE_IRRADIANCE_TEXELS - 1,
                                          PROBE_IRRADIANCE_TEXELS - 1)));
        int2 coord = tile * PROBE_IRRADIANCE_TEXELS + irrTexel;
        float2 uv = (float2(coord) + float2(0.5, 0.5)) /
                    float2(irrAtlasSize, irrAtlasSize);

        float vis = probeIrradiance.Sample(uv).a;
        float sw = triW * backface * (quality * quality);
        result += vis * sw;
        totalWeight += sw;
    }
    return totalWeight > 1e-4 ? result / totalWeight : 1.0;
}

// Probe-based radiance lookup along an arbitrary direction: trilinearly
// blends the stored per-direction radiance (the probe field captures the
// skybox on miss rays, so this replaces direct cubemap sampling).
float3 sampleProbeRadiance(float3 pos, float3 dir) {
    int3 counts = probeVolume.probeCounts.xyz;
    if (counts.x < 2 || counts.y < 2 || counts.z < 2)
        return float3(0.0, 0.0, 0.0);

    float3 origin = probeVolume.origin.xyz;
    float spacing = probeVolume.params.x;
    float tilesPerSide = probeVolume.atlas.x;
    float irrAtlasSize = tilesPerSide * float(PROBE_IRRADIANCE_TEXELS);

    float3 q = (pos - origin) / spacing;
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
        float triW = w.x * w.y * w.z;
        if (triW <= 0.0)
            continue;

        float2 oct = probeOctEncode(dir);
        int2 tile = int2(probeIndex % int(tilesPerSide),
                         probeIndex / int(tilesPerSide));
        int2 irrTexel = int2(clamp(oct * float(PROBE_IRRADIANCE_TEXELS),
                                   float2(0.0, 0.0),
                                   float2(PROBE_IRRADIANCE_TEXELS - 1,
                                          PROBE_IRRADIANCE_TEXELS - 1)));
        int2 coord = tile * PROBE_IRRADIANCE_TEXELS + irrTexel;
        float2 uv = (float2(coord) + float2(0.5, 0.5)) /
                    float2(irrAtlasSize, irrAtlasSize);

        result += probeIrradiance.Sample(uv).rgb * triW;
        totalWeight += triW;
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

// Percentage-Closer Filtering shadow calculation for a single directional light.
// Returns OCCLUSION (1 = shadowed, 0 = lit); the BRDF multiplies direct light
// by (1 - shadow). The depth test yields 1 for fragments closer to the light
// than the stored occluder, i.e. LIT, so that case must map to 0.
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
            shadow += (currentDepth - bias) < pcfDepth ? 0.0 : 1.0;
        }
    }
    shadow /= 9.0;
    return shadow;
}
// Cube map shadows for a point light (linear depth). slot indexes the cube
// shadow map array statically (callers unroll their loops). Returns OCCLUSION
// (1 = shadowed, 0 = lit), matching calculateShadow.
float calculatePointLightShadow(float3 lightPos, float3 fragPos, int slot) {
    if (ubo.pointShadowParams[slot].w <= 0.0) return 0.0;   // no shadowed point light

    float3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight) * ubo.pointShadowParams[slot].w;  // linear, [0,1]
    float bias = 0.02;                                                    // tune for acne
    float closestDepth = shadowCubeMaps[slot].Sample(fragToLight);        // linear depth
    return (currentDepth - bias) < closestDepth ? 0.0 : 1.0;
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

    // Emissive (rt.rgen parity): unshadowed, added on top of everything
    // including glass. The CPU zeroes the factor when no emissive texture is
    // bound (the descriptor falls back to an opaque texture), so the factor
    // doubles as the enable flag.
    float2 emissiveUv = applyKhrTextureTransform(input.fragTexCoord,
        material.emissiveUvOx, material.emissiveUvOy, material.emissiveUvRot,
        material.emissiveUvSx, material.emissiveUvSy);
    float3 emissive = emissiveSampler.Sample(emissiveUv).rgb *
        float3(material.emissiveR, material.emissiveG, material.emissiveB);

    // Glass (KHR_materials_transmission): fresnel-weighted alpha - grazing
    // angles reflect (opaque-ish edges), head-on transmits (transparent).
    float transmission = material.transmissionFactor;
    float alpha = texSample.a * material.alpha;
    float3 gi = sampleProbeGI(input.fragWorldPos, normal) * probeVolume.debug.y;
    float3 glassColor = float3(0.0, 0.0, 0.0);
    if (transmission > 0.0) {
        float cosTheta = max(dot(normal, viewDir), 0.0);
        float iorF0 = (material.idxReflect - 1.0) / (material.idxReflect + 1.0);
        iorF0 *= iorF0;
        float F = iorF0 + (1.0 - iorF0) * pow(1.0 - cosTheta, 5.0);
        alpha = lerp(alpha, 1.0 - transmission * (1.0 - F), transmission);

        // Glass has no diffuse; like the path tracer (rt.rgen) its color is
        // the environment seen through/off the surface: probe-field radiance
        // sampled along the reflection direction (the probes capture the
        // skybox on miss rays, so no direct cubemap sampling is needed),
        // absorbed through the volume by Beer-Lambert.
        float3 reflDir = reflect(-viewDir, normal);
        float3 env = sampleProbeRadiance(input.fragWorldPos, reflDir);
        // Floor with the surface GI so glass never goes fully black in
        // regions the probe field has not captured yet.
        env = max(env, gi * 0.5 / 3.14159265 + float3(0.02, 0.02, 0.02));
        float thickness = max(material.thicknessFactor, 0.0);
        float3 atten = pow(float3(material.attenuationR, material.attenuationG,
                                  material.attenuationB),
                           GLASS_ATTENUATION_SOFTEN * thickness /
                           max(material.attenuationDistance, 1e-3));
        glassColor = transmission * env * atten;

        // Don't let dark reflections drive the glass fully opaque: cap the
        // opacity by the brightness of the environment behind it.
        float envLum = max(max(env.r, env.g), env.b);
        alpha = min(alpha, clamp(envLum * 3.0, 0.08, 1.0));
    }

    float metallic = material.metallicFactor;
    // Floor the roughness: roughness 0 collapses the GGX distribution to a
    // zero-width spike, killing specular highlights entirely (glass depends
    // on them for its look). Matches rt.rgen's floor (0.001).
    float roughness = max(material.roughnessFactor, 0.001);

    // BRDF probe sampling: diffuse = cosine-weighted hemisphere taps,
    // specular = roughness-cone taps around the reflection direction. Tap
    // count is the live quality knob (probeVolume.shadow.z); 1 tap falls
    // back to the legacy single-direction lookup.
    int giTaps = clamp(int(probeVolume.shadow.z), 1, 32);
    float3 giDiffuse = sampleProbeDiffuse(input.fragWorldPos, normal, giTaps) *
                       probeVolume.debug.y;
    float3 giSpecular =
        sampleProbeSpecular(input.fragWorldPos, normal, viewDir, roughness,
                            max(giTaps / 2, 1)) *
        probeVolume.debug.y;

    float shadow = calculateShadow(input.fragLightSpacePos);
    // Cosine-weighted taps make the pdf cancel: outgoing diffuse radiance is
    // albedo * mean tap radiance (the field stores cosine-weighted radiance).
    float3 indirectDiffuse = ubo.ambientStrength * albedo + giDiffuse * albedo;

    // KHR_materials_diffuse_transmission: the diffuse lobe is split between
    // reflection and transmission, so the reflected side scales by (1 - dt).
    // Glass (transmission) has no diffuse response at all.
    float dt = material.diffuseTransmissionFactor;
    float3 ambient = indirectDiffuse * (1.0 - dt) * (1.0 - transmission);

    // Transmitted side: approximate the light coming through the surface
    // (the raster path has no behind-scene buffer to sample). Beer-Lambert
    // absorption through the volume (KHR_materials_volume).
    float thickness = max(material.thicknessFactor, 0.0);
    float3 atten = pow(float3(material.attenuationR, material.attenuationG, material.attenuationB),
                       thickness / max(material.attenuationDistance, 1e-3));
    float3 transmitted = indirectDiffuse * atten *
                         float3(material.diffuseTransmissionR,
                                material.diffuseTransmissionG,
                                material.diffuseTransmissionB);
    ambient += dt * (1.0 - transmission) * transmitted;
    float3 lighting = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < ubo.numLights && i < MAX_LIGHTS; i++) {
        int lightType = int(ubo.lights[i].attenuation.w);
        if (lightType == 0) {
            // Glass is rendered as an opaque caster into the shadow map, so
            // its interior faces pick up self-shadow patches that read as
            // random black artifacts once the surface is transparent.
            // Transmissive materials skip shadows (matches the RT glass path).
            float dirShadow = (transmission > 0.0) ? 0.0 : shadow;
            // Probe-stored shadow information: blend the soft probe shadow
            // into the shadow-map term (probeVolume.shadow.x = strength).
            // sampleProbeShadow returns VISIBILITY (1 = lit), dirShadow is
            // occlusion, so invert before blending.
            if (transmission <= 0.0 && probeVolume.shadow.x > 0.001) {
                float probeShadow = sampleProbeShadow(
                    input.fragWorldPos,
                    -normalize(ubo.lights[i].direction.xyz), normal);
                dirShadow = lerp(dirShadow, 1.0 - probeShadow,
                                 clamp(probeVolume.shadow.x, 0.0, 1.0));
            }
            lighting += calculateDirectionalLight(ubo.lights[i], normal, viewDir, albedo, metallic, roughness, dirShadow);
        } else if (lightType == 1) {
            // Point lights with cube shadows are handled by the unrolled
            // loop below (static sampler-array indexing); skip them here.
            continue;
        } else if (lightType == 2) {
            lighting += calculateSpotLight(ubo.lights[i], normal, viewDir, input.fragWorldPos, albedo, metallic, roughness);
        }
    }

    // Point lights with cube shadow maps. Unrolled so the sampler array
    // index (k) is compile-time constant - Vulkan requires static indexing
    // for resource arrays.
    [unroll]
    for (int k = 0; k < MAX_POINT_SHADOWS; k++) {
        if (k >= ubo.numPointLights) break;
        float pointShadow = calculatePointLightShadow(
            ubo.pointLights[k].position.xyz, input.fragWorldPos, k);
        if (transmission > 0.0) pointShadow = 0.0;
        lighting += calculatePointLight(ubo.pointLights[k], normal, viewDir,
                                        input.fragWorldPos, albedo, metallic,
                                        roughness, pointShadow);
    }

    // Like the path tracer (rt.rgen), transmissive surfaces skip the direct
    // light response: glass is purely environment-driven.
    // Probe-field specular: Fresnel modulates the cone-averaged reflection
    // (F0 = albedo for metals, so reflections tint with the base color).
    float3 F0gi = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 Fgi = fresnelWithIridescence(max(dot(normal, viewDir), 0.0), F0gi);
    float3 result = (transmission > 0.0)
        ? (glassColor + emissive)
        : (ambient + lighting + giSpecular * Fgi * (1.0 - dt) + emissive);

    // Clear coat layering (on top of base BRDF). Skipped for glass: the
    // coat fresnel zeroes the result at grazing angles, which combined with
    // fresnel alpha made opaque black patches on transmissive surfaces.
    if (material.clearCoatFactor > 0.0 && transmission <= 0.0) {
        float ccRoughness = material.clearCoatRoughness;
        float3 ccSpec = 0.0;
        [unroll] for (int i = 0; i < ubo.numLights && i < MAX_LIGHTS; i++) {
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

    // Probe debug visualization (probeVolume.debug.x):
    //   1 = GI only, 2 = GI heatmap, 3 = probe grid cells.
    if (probeVolume.debug.x >= 0.5) {
        float3 dbg;
        if (probeVolume.debug.x < 1.5) {
            dbg = gi * albedo / 3.14159265;  // indirect term only
        } else if (probeVolume.debug.x < 2.5) {
            float3 v = gi * albedo / 3.14159265;
            float t = clamp((v.r + v.g + v.b) / 3.0 * 2.0, 0.0, 1.0);
            dbg = float3(clamp(1.5 - abs(t * 4.0 - 3.0), 0.0, 1.0),
                         clamp(1.5 - abs(t * 4.0 - 2.0), 0.0, 1.0),
                         clamp(1.5 - abs(t * 4.0 - 1.0), 0.0, 1.0));
        } else if (probeVolume.debug.x < 3.5) {
            // Color by the fractional position inside the probe cell: shows
            // the grid density and where cell boundaries fall on surfaces.
            float3 q = (input.fragWorldPos - probeVolume.origin.xyz) /
                       max(probeVolume.params.x, 1e-4);
            dbg = frac(q);
        } else {
            // 4 = probe quality field: 1 (green) = healthy probe, 0 (red) =
            // stuck inside geometry. Red regions are the probes whose
            // garbage radiance used to leak into the shading.
            float3 q4 = (input.fragWorldPos - probeVolume.origin.xyz) /
                        max(probeVolume.params.x, 1e-4);
            float3 qc4 = clamp(q4, float3(0.5, 0.5, 0.5),
                               float3(probeVolume.probeCounts.xyz) -
                                   float3(0.5001, 0.5001, 0.5001));
            float3 basef4 = qc4 - float3(0.5, 0.5, 0.5);
            int3 base4 = int3(floor(basef4));
            float3 frac4 = basef4 - float3(base4);
            float qsum = 0.0;
            float qwsum = 0.0;
            for (int dz = 0; dz <= 1; dz++)
            for (int dy = 0; dy <= 1; dy++)
            for (int dx = 0; dx <= 1; dx++) {
                int3 idx4 = base4 + int3(dx, dy, dz);
                int pi = (idx4.z * probeVolume.probeCounts.y + idx4.y) *
                             probeVolume.probeCounts.x +
                         idx4.x;
                float3 w4 = float3(1.0, 1.0, 1.0) -
                            abs(float3(dx, dy, dz) - frac4);
                float tw = w4.x * w4.y * w4.z;
                if (tw <= 0.0)
                    continue;
                qsum += probePositions[uint(pi)].rotationSeed.z * tw;
                qwsum += tw;
            }
            float qv = qwsum > 0.0 ? qsum / qwsum : 1.0;
            dbg = lerp(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0),
                       clamp(qv, 0.0, 1.0));
        }
        output.outColor = float4(dbg, 1.0);
        return output;
    }

    // Reinhard tone mapping + sRGB encode (matches rt.rgen exactly)
    result = result / (result + float3(1.0, 1.0, 1.0));
    float3 srgbT = step(0.0031308, result);
    result = lerp(result * 12.92,
                  pow(result, float3(1.0 / 2.4, 1.0 / 2.4, 1.0 / 2.4)) * 1.055 - 0.055,
                  srgbT);
    output.outColor = float4(result, alpha);
    return output;
}
