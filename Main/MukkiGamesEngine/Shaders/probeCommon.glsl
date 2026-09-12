// Shared DDGI helpers: octahedral mapping, probe sampling, visibility.
// Pure functions only - no uniforms. Each consumer shader declares its own
// bindings and passes values as arguments (keeps raster/RT reuse trivial).

const int MAX_LIGHTS = 4;
const int LIGHT_TYPE_DIRECTIONAL = 0;
const int LIGHT_TYPE_POINT = 1;
const int LIGHT_TYPE_SPOT = 2;

// Must match ProbeVolume's defaults (vulkan/Resources/ProbeVolume.h).
const int PROBE_IRRADIANCE_TEXELS = 8;
const int PROBE_DEPTH_TEXELS      = 16;
const int PROBE_RAYS_PER_PROBE    = 256;

// Octahedral encode: unit vector -> [0,1]^2
vec2 octEncode(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    }
    return n.xy * 0.5 + 0.5;
}

// Octahedral decode: [0,1]^2 -> unit vector
vec3 octDecode(vec2 e) {
    e = e * 2.0 - 1.0;
    vec3 n = vec3(e.x, e.y, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    }
    return normalize(n);
}

// Evenly-distributed direction i of n (better than octahedral texel centers)
vec3 sphericalFibonacci(uint i, uint n) {
    float phi    = acos(1.0 - 2.0 * (float(i) + 0.5) / float(n));
    float golden = 3.14159265358979 * (3.0 - sqrt(5.0));
    float theta  = golden * float(i);
    return vec3(cos(theta) * sin(phi), sin(theta) * sin(phi), cos(phi));
}

// Deterministic per-probe rotation from the two random seed uniforms.
// Breaks up structured ray patterns so adjacent probes decorrelate.
vec3 rotateDir(vec3 dir, vec2 seed) {
    float a = seed.x * 6.283185307179586;
    float b = seed.y * 6.283185307179586;
    float ca = cos(a), sa = sin(a);
    float cb = cos(b), sb = sin(b);
    vec3 t = vec3(cb * dir.x + sb * dir.z, dir.y, -sb * dir.x + cb * dir.z);
    return vec3(ca * t.x - sa * t.y, sa * t.x + ca * t.y, t.z);
}

// One-tailed Chebyshev visibility: 0 = fully occluded, 1 = visible.
// moments = (mean depth, mean depth^2) from the probe depth atlas.
float chebyshevWeight(vec2 moments, float depth) {
    float mean     = moments.x;
    float variance = max(moments.y - mean * mean, 0.0001);
    float d = depth - mean;
    float p = (d > 0.0) ? variance / (variance + d * d) : 1.0;
    return max(p * p * p, 0.05);
}

// Trilinear + octahedral + Chebyshev probe lookup.
// This is THE function every consumer uses (probe feedback + final shading).
vec3 sampleProbeIrradiance(sampler2D irrTex, sampler2D depthTex,
                           vec3 pos, vec3 normal,
                           vec3 volumeOrigin, float probeSpacing, ivec3 probeCounts,
                           float tilesPerSide, float normalBias)
{
    // Bias the lookup point off the surface so the shading point's own
    // probe doesn't self-shadow the sample.
    vec3 biasedPos = pos + normal * normalBias;

    vec3 q  = (biasedPos - volumeOrigin) / probeSpacing;
    // Clamp into the grid interior so edge probes still have 8 neighbors.
    vec3 qc = clamp(q, vec3(0.5), vec3(probeCounts) - vec3(0.5001));
    vec3 basef = qc - vec3(0.5);
    ivec3 base = ivec3(floor(basef));
    vec3 frac  = basef - vec3(base);

    vec3  result      = vec3(0.0);
    float totalWeight = 0.0;

    for (int dz = 0; dz <= 1; dz++)
    for (int dy = 0; dy <= 1; dy++)
    for (int dx = 0; dx <= 1; dx++) {
        ivec3 idx = base + ivec3(dx, dy, dz);
        uint probeIndex = uint((idx.z * probeCounts.y + idx.y) * probeCounts.x + idx.x);

        vec3 w = vec3(1.0) - abs(vec3(dx, dy, dz) - frac);
        float weight = w.x * w.y * w.z;
        if (weight <= 0.0) continue;

        vec3  probePos    = volumeOrigin + (vec3(idx) + vec3(0.5)) * probeSpacing;
        vec3  toProbe     = probePos - biasedPos;
        float distToProbe = length(toProbe);
        vec3  dirToProbe  = toProbe / max(distToProbe, 1e-5);

        vec2 oct = octEncode(dirToProbe);
        ivec2 irrTexel   = ivec2(clamp(oct * float(PROBE_IRRADIANCE_TEXELS), vec2(0.0), vec2(PROBE_IRRADIANCE_TEXELS - 1.0)));
        ivec2 depthTexel = ivec2(clamp(oct * float(PROBE_DEPTH_TEXELS),      vec2(0.0), vec2(PROBE_DEPTH_TEXELS - 1.0)));

        uvec2 tile = uvec2(probeIndex % uint(tilesPerSide), probeIndex / uint(tilesPerSide));
        ivec2 irrCoord   = ivec2(tile) * PROBE_IRRADIANCE_TEXELS + irrTexel;
        ivec2 depthCoord = ivec2(tile) * PROBE_DEPTH_TEXELS      + depthTexel;

        vec3  irr        = texelFetch(irrTex,   irrCoord,   0).rgb;
        vec2  moments    = texelFetch(depthTex, depthCoord, 0).rg;
        float visibility = chebyshevWeight(moments, distToProbe);

        result      += irr * (weight * visibility);
        totalWeight += weight * visibility;
    }
    return totalWeight > 0.0 ? result / totalWeight : vec3(0.0);
}
