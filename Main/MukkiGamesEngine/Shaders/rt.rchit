#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
struct Payload
{
    vec3 color;
    vec3 position;
    vec3 normal;
    int hit;
    int shadowRay;
    float metallic;
    float roughness;
    vec3 emissiveColor;
    float transmission;
    float idxReflect;
    int frontFace;
    float hitT;
    float attenuationR;
    float attenuationG;
    float attenuationB;
    float attenuationDistance;
    float dispersion;
    float iridescenceFactor;
    float iridescenceIor;
    float iridescenceMin;
    float iridescenceMax;
    float iridescenceThickness;
    float diffuseTransmissionFactor;
    float diffuseTransmission;
    float diffuseTransmissionR;
    float diffuseTransmissionG;
    float diffuseTransmissionB;
    float scatteringR;
    float scatteringG;
    float scatteringB;
    float scatteringDistance;
    float scatteringAnisotropy;
    float scatteringRange;
};

layout(location = 0) rayPayloadInEXT Payload payload;

struct RayTracingVertex
{
    vec4 position;
    vec4 normal;
    vec2 texCoord;
    float pad0;
    float pad1;
};

struct PrimitiveInfo
{
    uint firstIndex;
    uint indexCount;
    int textureIndex;
    float metallicFactor;
    float roughnessFactor;
    float baseColorR;
    float baseColorG;
    float baseColorB;
    float emissiveR;
    float emissiveG;
    float emissiveB;
    uint vertexOffset;
    int emssiveTextureIndex;
    float transmissionFactor;
    float idxReflect;
    float attenuationR;
	float attenuationG;
	float attenuationB;
	float attenuationDistance;
	float dispersion;
	float iridescenceFactor;
    float iridescenceIor;
    float iridescenceMin;
    float iridescenceMax;
    float diffuseTransmissionFactor;
    float diffuseTransmissionR;
    float diffuseTransmissionG;
    float diffuseTransmissionB;
    int diffuseTransmissionTextureIndex;
    float scatteringR;
    float scatteringG;
    float scatteringB;
    float scatteringDistance;
    float scatteringAnisotropy;
    float scatteringRange;
    int metallicRoughnessTextureIndex;
    int iridescenceThicknessTextureIndex;
    // KHR_texture_transform per slot: offsetX, offsetY, rotation, scaleX, scaleY
    float baseColorUvOx; float baseColorUvOy; float baseColorUvRot; float baseColorUvSx; float baseColorUvSy;
    float metallicRoughnessUvOx; float metallicRoughnessUvOy; float metallicRoughnessUvRot; float metallicRoughnessUvSx; float metallicRoughnessUvSy;
    float emissiveUvOx; float emissiveUvOy; float emissiveUvRot; float emissiveUvSx; float emissiveUvSy;
    float iridescenceUvOx; float iridescenceUvOy; float iridescenceUvRot; float iridescenceUvSx; float iridescenceUvSy;
    float diffuseTransmissionUvOx; float diffuseTransmissionUvOy; float diffuseTransmissionUvRot; float diffuseTransmissionUvSx; float diffuseTransmissionUvSy;
};

struct MeshInfo
{
    uint primitiveOffset;
    uint primitiveCount;
    uint pad0;
    uint pad1;
};

layout(set = 0, binding = 3, std430) readonly buffer IndexBuffer
{
    uint indices[];
} indexBuffer;

layout(set = 0, binding = 4, std430) readonly buffer VertexBuffer
{
    RayTracingVertex vertices[];
} vertexBuffer;

layout(set = 0, binding = 5, std430) readonly buffer PrimitiveBuffer
{
    PrimitiveInfo primitives[];
} primitiveBuffer;

layout(set = 0, binding = 6, std430) readonly buffer MeshBuffer
{
    MeshInfo meshes[];
} meshBuffer;

layout(set = 0, binding = 8) uniform sampler2D textures[100];

hitAttributeEXT vec2 attribs;

vec2 applyKhrTextureTransform(vec2 uv, float ox, float oy, float rot, float sx, float sy)
{
    float c = cos(rot);
    float s = sin(rot);
    return vec2(c * sx * uv.x - s * sy * uv.y + ox,
                s * sx * uv.x + c * sy * uv.y + oy);
}

void main()
{
    payload.hit = 1;
    if(payload.shadowRay !=0){
        return;
    }
    payload.position = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

    uint meshIndex = gl_InstanceCustomIndexEXT;
    MeshInfo meshInfo = meshBuffer.meshes[meshIndex];
    uint primitiveIndex = meshInfo.primitiveOffset + gl_GeometryIndexEXT;
    PrimitiveInfo primInfo = primitiveBuffer.primitives[primitiveIndex];

    uint triIndex = primInfo.firstIndex + uint(gl_PrimitiveID) * 3u;
    uint i0 = indexBuffer.indices[triIndex + 0u] + primInfo.vertexOffset;
    uint i1 = indexBuffer.indices[triIndex + 1u] + primInfo.vertexOffset;
    uint i2 = indexBuffer.indices[triIndex + 2u] + primInfo.vertexOffset;

    RayTracingVertex v0 = vertexBuffer.vertices[i0];
    RayTracingVertex v1 = vertexBuffer.vertices[i1];
    RayTracingVertex v2 = vertexBuffer.vertices[i2];

    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 normal = normalize(v0.normal.xyz * bary.x + v1.normal.xyz * bary.y + v2.normal.xyz * bary.z);
    mat3 normalMatrix = transpose(mat3(gl_WorldToObjectEXT));
    normal = normalize(normalMatrix * normal);
    payload.frontFace = dot(normal, gl_WorldRayDirectionEXT) < 0.0 ? 1 : 0;
    if (dot(normal, gl_WorldRayDirectionEXT) > 0.0)
    {
        normal = -normal;
    }

    vec2 uv = v0.texCoord * bary.x + v1.texCoord * bary.y + v2.texCoord * bary.z;
    vec2 uvBase = applyKhrTextureTransform(uv,
        primInfo.baseColorUvOx, primInfo.baseColorUvOy,
        primInfo.baseColorUvRot, primInfo.baseColorUvSx, primInfo.baseColorUvSy);
    vec2 uvMr = applyKhrTextureTransform(uv,
        primInfo.metallicRoughnessUvOx, primInfo.metallicRoughnessUvOy,
        primInfo.metallicRoughnessUvRot, primInfo.metallicRoughnessUvSx, primInfo.metallicRoughnessUvSy);
    vec2 uvEm = applyKhrTextureTransform(uv,
        primInfo.emissiveUvOx, primInfo.emissiveUvOy,
        primInfo.emissiveUvRot, primInfo.emissiveUvSx, primInfo.emissiveUvSy);
    vec2 uvIrid = applyKhrTextureTransform(uv,
        primInfo.iridescenceUvOx, primInfo.iridescenceUvOy,
        primInfo.iridescenceUvRot, primInfo.iridescenceUvSx, primInfo.iridescenceUvSy);
    vec2 uvDt = applyKhrTextureTransform(uv,
        primInfo.diffuseTransmissionUvOx, primInfo.diffuseTransmissionUvOy,
        primInfo.diffuseTransmissionUvRot, primInfo.diffuseTransmissionUvSx, primInfo.diffuseTransmissionUvSy);
    int texIdx = primInfo.textureIndex;
    vec3 albedo;
    vec3 baseColor = vec3(primInfo.baseColorR, primInfo.baseColorG, primInfo.baseColorB);
    if (texIdx >= 0) {
        albedo = texture(textures[nonuniformEXT(texIdx)], uvBase).rgb * baseColor;
    } else {
        albedo = baseColor;
    }
    vec3 emissiveColor = vec3(primInfo.emissiveR, primInfo.emissiveG, primInfo.emissiveB);
    int emissiveTexIdx = primInfo.emssiveTextureIndex;
    if(emissiveTexIdx >= 0) {
        emissiveColor *= texture(textures[nonuniformEXT(emissiveTexIdx)], uvEm).rgb;
    }
    payload.normal = normal;
    payload.color = albedo;
    int mrTexIdx = primInfo.metallicRoughnessTextureIndex;
    float metallic = primInfo.metallicFactor;
    float roughness = primInfo.roughnessFactor;
    if (mrTexIdx >= 0) {
        vec3 mr = texture(textures[nonuniformEXT(mrTexIdx)], uvMr).rgb;
        metallic *= mr.b;   // glTF: B = metallic
        roughness *= mr.g;  // glTF: G = roughness
    }
    payload.metallic = metallic;
    payload.roughness = roughness;

    payload.emissiveColor = emissiveColor;
    payload.transmission = primInfo.transmissionFactor;
    payload.idxReflect = primInfo.idxReflect;
    payload.hitT = gl_HitTEXT;
    payload.attenuationR = primInfo.attenuationR;
    payload.attenuationG = primInfo.attenuationG;
    payload.attenuationB = primInfo.attenuationB;
    payload.attenuationDistance = primInfo.attenuationDistance;
    payload.dispersion = primInfo.dispersion;
    payload.iridescenceFactor = primInfo.iridescenceFactor;
    payload.iridescenceIor = primInfo.iridescenceIor;
    payload.iridescenceMin = primInfo.iridescenceMin;
    payload.iridescenceMax = primInfo.iridescenceMax;
    float iridThickness = mix(primInfo.iridescenceMin, primInfo.iridescenceMax, 0.5);
    int iridTexIdx = primInfo.iridescenceThicknessTextureIndex;
    if (iridTexIdx >= 0) {
        iridThickness = mix(primInfo.iridescenceMin, primInfo.iridescenceMax,
                            texture(textures[nonuniformEXT(iridTexIdx)], uvIrid).r);
    }
    payload.iridescenceThickness = iridThickness;
    payload.diffuseTransmission = primInfo.diffuseTransmissionFactor;
    if (primInfo.diffuseTransmissionTextureIndex >= 0) {
        payload.diffuseTransmission *=
            texture(textures[nonuniformEXT(primInfo.diffuseTransmissionTextureIndex)], uvDt).r;
    }
    payload.diffuseTransmissionR = primInfo.diffuseTransmissionR;
    payload.diffuseTransmissionG = primInfo.diffuseTransmissionG;
    payload.diffuseTransmissionB = primInfo.diffuseTransmissionB;
    payload.scatteringR = primInfo.scatteringR;
    payload.scatteringG = primInfo.scatteringG;
    payload.scatteringB = primInfo.scatteringB;
    payload.scatteringDistance = primInfo.scatteringDistance;
    payload.scatteringAnisotropy = primInfo.scatteringAnisotropy;
    payload.scatteringRange = primInfo.scatteringRange;
}
