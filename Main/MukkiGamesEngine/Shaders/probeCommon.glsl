// Shared DDGI helpers: octahedral mapping, probe sampling, visibility.
// Pure functions only - no uniforms. Each consumer shader declares its own
// bindings and passes values as arguments (keeps raster/RT reuse trivial).

const int MAX_LIGHTS = 8;
const int LIGHT_TYPE_DIRECTIONAL = 0;
const int LIGHT_TYPE_POINT = 1;
const int LIGHT_TYPE_SPOT = 2;

// Must match ProbeVolume's defaults (vulkan/Resources/ProbeVolume.h).
const int PROBE_IRRADIANCE_TEXELS = 8;
// Depth texels are intentionally coarser than the ray count would allow:
// 256 rays into an 8x8 map gives ~4 samples per texel, so the depth moments
// carry an actual variance. At 16x16 it was 1 sample/texel, the variance
// collapsed to zero, and the Chebyshev test became a knife edge.
const int PROBE_DEPTH_TEXELS      = 8;
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
// The variance is floored RELATIVE to the mean. The depth moments come from
// a handful of rays per texel, so a texel can have (near) zero sample
// variance; the raw test then rejects any query point more than a few cm
// off the stored hit distance, which turns whole surface patches into the
// nearest-probe fallback (flat, straight-edged blotches). A 5% relative
// floor keeps the falloff smooth without re-introducing the old constant
// 0.05 visibility floor that let fully occluded probes leak.
float chebyshevWeight(vec2 moments, float depth) {
    float mean     = moments.x;
    float variance = max(moments.y - mean * mean, 0.05 * mean * mean + 0.0001);
    float d = depth - mean;
    float p = (d > 0.0) ? variance / (variance + d * d) : 1.0;
    return p * p * p;
}

// std430 mirror of ProbeData (vulkan/Resources/ProbeVolume.h). Holds the
// (possibly relocated) probe positions. Relocated probes must be sampled at
// their ACTUAL positions: the stored octahedral texel directions and depth
// moments are measured from the relocated origin, so grid-based lookup
// produces blotchy, mis-indexed shading.
struct ProbeData {
    vec4 pos;          // xyz = probe world position
    vec4 rotationSeed; // xy = ray rotation seed, z = probe quality
                       // (1 = healthy, 0 = stuck inside geometry)
};
