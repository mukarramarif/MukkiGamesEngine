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
    int metallicRoughnessTextureIndex;
    int iridescenceThicknessTextureIndex;
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
    int texIdx = primInfo.textureIndex;
    vec3 albedo;
    vec3 baseColor = vec3(primInfo.baseColorR, primInfo.baseColorG, primInfo.baseColorB);
    if (texIdx >= 0) {
        albedo = texture(textures[nonuniformEXT(texIdx)], uv).rgb * baseColor;
    } else {
        albedo = baseColor;
    }
    vec3 emissiveColor = vec3(primInfo.emissiveR, primInfo.emissiveG, primInfo.emissiveB);
    int emissiveTexIdx = primInfo.emssiveTextureIndex;
    if(emissiveTexIdx >= 0) {
        emissiveColor *= texture(textures[nonuniformEXT(emissiveTexIdx)], uv).rgb;
    }
    payload.normal = normal;
    payload.color = albedo;
    int mrTexIdx = primInfo.metallicRoughnessTextureIndex;
    float metallic = primInfo.metallicFactor;
    float roughness = primInfo.roughnessFactor;
    if (mrTexIdx >= 0) {
        vec3 mr = texture(textures[nonuniformEXT(mrTexIdx)], uv).rgb;
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
                            texture(textures[nonuniformEXT(iridTexIdx)], uv).r);
    }
    payload.iridescenceThickness = iridThickness;
}
