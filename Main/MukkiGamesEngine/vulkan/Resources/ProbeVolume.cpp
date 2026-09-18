#include "ProbeVolume.h"
#include <stdexcept>
#include <random>
#include <cmath>
#include <cstring>
#include <vector>

ProbeVolume::~ProbeVolume() { cleanup(); }

bool ProbeVolume::init(Device* deviceIn,
                       const glm::ivec3& probeCounts,
                       float probeSpacing,
                       const glm::vec3& origin,
                       uint32_t irradianceTexelsPerProbe,
                       uint32_t depthTexelsPerProbe)
{
    if (probeCounts.x < 1 || probeCounts.y < 1 || probeCounts.z < 1)
        throw std::runtime_error("probe volume counts must be >= 1 on every axis!");
    if (probeSpacing <= 0.0f)
        throw std::runtime_error("probe spacing must be positive!");

    device = deviceIn;
    m_irradianceTexels = irradianceTexelsPerProbe;
    m_depthTexels      = depthTexelsPerProbe;

    m_probeCount   = static_cast<uint32_t>(probeCounts.x * probeCounts.y * probeCounts.z);
    m_tilesPerSide = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(m_probeCount))));

    // Defaults: maxRayDistance = 4x spacing, hysteresis 0.97, normalBias 0.25
    m_params = {};
    m_params.origin      = glm::vec4(origin, 0.0f);
    m_params.probeCounts = glm::ivec4(probeCounts, static_cast<int>(m_probeCount));
    m_params.params      = glm::vec4(probeSpacing, probeSpacing * 4.0f, 0.97f, 0.25f);
    m_params.debug       = glm::vec4(0.0f, 1.0f, 1.0f, 0.5f); // mode off, GI strength 1, relocation on, feedback gain 0.5
    m_params.shadow      = glm::vec4(0.0f, 1.0f, 8.0f, 0.0f); // probe shadows off, BRDF taps = 8
    m_params.atlas       = glm::vec4(static_cast<float>(m_tilesPerSide),
                                     static_cast<float>(m_irradianceTexels),
                                     static_cast<float>(m_depthTexels),
                                     static_cast<float>(DEFAULT_RAYS_PER_PROBE));

    createAtlasImages(VK_FORMAT_R16G16B16A16_SFLOAT, m_irradianceTexels,
                      m_irradianceImages, m_irradianceMemories, m_irradianceViews);
    createAtlasImages(VK_FORMAT_R16G16_SFLOAT, m_depthTexels,
                      m_depthImages, m_depthMemories, m_depthViews);
    createSampler();
    createProbeDataBuffer();
    createParamsBuffer();
    return true;
}

void ProbeVolume::volumeFromAABB(const glm::vec3& sceneMin, const glm::vec3& sceneMax,
                                 float probeSpacing,
                                 glm::ivec3& outCounts, glm::vec3& outOrigin)
{
    const glm::vec3 size = sceneMax - sceneMin;
    // +1 so the grid fully encloses the AABB, min 2 probes per axis
    outCounts = glm::max(glm::ivec3(2), glm::ivec3(glm::ceil(size / probeSpacing)) + glm::ivec3(1));
    // Center the grid on the AABB
    const glm::vec3 gridSize = glm::vec3(outCounts - 1) * probeSpacing;
    outOrigin = (sceneMin + sceneMax) * 0.5f - gridSize * 0.5f;
}

void ProbeVolume::setHysteresis(float hysteresis)    { m_params.params.z = hysteresis;    updateParams(); }
void ProbeVolume::setNormalBias(float normalBias)    { m_params.params.w = normalBias;    updateParams(); }
void ProbeVolume::setMaxRayDistance(float maxRayDistance)   { m_params.params.y = maxRayDistance;       updateParams(); }
void ProbeVolume::setDebugMode(int mode)             { m_params.debug.x = static_cast<float>(mode); updateParams(); }
void ProbeVolume::setGIStrength(float strength)      { m_params.debug.y = strength;       updateParams(); }
void ProbeVolume::setRelocationEnabled(bool enabled) { m_params.debug.z = enabled ? 1.0f : 0.0f; updateParams(); }
void ProbeVolume::setFeedbackGain(float gain)        { m_params.debug.w = gain;           updateParams(); }
void ProbeVolume::setProbeShadowStrength(float s)    { m_params.shadow.x = s;             updateParams(); }
void ProbeVolume::setBRDFTaps(int taps)              { m_params.shadow.z = static_cast<float>(taps); updateParams(); }

bool ProbeVolume::getProbePositionsCPU(std::vector<glm::vec4>& outPositions) const
{
    if (!device || m_probeDataBuffer == VK_NULL_HANDLE || m_probeCount == 0)
        return false;

    const VkDeviceSize size = sizeof(ProbeData) * m_probeCount;
    void* data = nullptr;
    if (vkMapMemory(device->getDevice(), m_probeDataMemory, 0, size, 0, &data) != VK_SUCCESS)
        return false;

    outPositions.resize(m_probeCount);
    const ProbeData* probes = static_cast<const ProbeData*>(data);
    for (uint32_t i = 0; i < m_probeCount; ++i)
        outPositions[i] = probes[i].pos;

    vkUnmapMemory(device->getDevice(), m_probeDataMemory);
    return true;
}

glm::vec3 ProbeVolume::gridPositionForIndex(uint32_t index) const
{
    const glm::ivec3 counts = glm::ivec3(m_params.probeCounts);
    const uint32_t x = index % static_cast<uint32_t>(counts.x);
    const uint32_t y = (index / static_cast<uint32_t>(counts.x)) % static_cast<uint32_t>(counts.y);
    const uint32_t z = index / (static_cast<uint32_t>(counts.x) * static_cast<uint32_t>(counts.y));
    return glm::vec3(m_params.origin) +
           (glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)) + 0.5f) *
               m_params.params.x;
}

VkImageView ProbeVolume::getIrradianceView(uint32_t index) const { return m_irradianceViews[index]; }
VkImageView ProbeVolume::getDepthView(uint32_t index)      const { return m_depthViews[index]; }
VkImage     ProbeVolume::getIrradianceImage(uint32_t index) const { return m_irradianceImages[index]; }
VkImage     ProbeVolume::getDepthImage(uint32_t index)      const { return m_depthImages[index]; }

void ProbeVolume::clear(VkCommandBuffer cmd)
{
    if (!device) return;
    VkClearColorValue clearValue{};
    VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    for (uint32_t i = 0; i < PING_PONG_COUNT; i++) {
        vkCmdClearColorImage(cmd, m_irradianceImages[i], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &range);
        vkCmdClearColorImage(cmd, m_depthImages[i],      VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &range);
    }
}

void ProbeVolume::createAtlasImages(VkFormat format, uint32_t texelsPerProbe,
                                    std::array<VkImage, PING_PONG_COUNT>& images,
                                    std::array<VkDeviceMemory, PING_PONG_COUNT>& memories,
                                    std::array<VkImageView, PING_PONG_COUNT>& views)
{
    const uint32_t atlasDim = m_tilesPerSide * texelsPerProbe;

    for (uint32_t i = 0; i < PING_PONG_COUNT; i++) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = { atlasDim, atlasDim, 1 };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // vkCmdClearColorImage
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device->getDevice(), &imageInfo, nullptr, &images[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create probe atlas image!");

        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(device->getDevice(), images[i], &memReq);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex =
            device->findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &memories[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate probe atlas memory!");
        vkBindImageMemory(device->getDevice(), images[i], memories[i], 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device->getDevice(), &viewInfo, nullptr, &views[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create probe atlas view!");
    }
}

void ProbeVolume::createSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;   // probe access is texelFetch anyway
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    if (vkCreateSampler(device->getDevice(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
        throw std::runtime_error("failed to create probe atlas sampler!");
}

void ProbeVolume::createProbeDataBuffer()
{
    const VkDeviceSize size = sizeof(ProbeData) * m_probeCount;
    device->createBuffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         m_probeDataBuffer, m_probeDataMemory);

    std::vector<ProbeData> probes(m_probeCount);
    std::mt19937 rng(1337);  // fixed seed: deterministic probe rotations
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    const glm::ivec3 counts = glm::ivec3(m_params.probeCounts);
    const float spacing = m_params.params.x;
    const glm::vec3 origin = glm::vec3(m_params.origin);
    for (int z = 0; z < counts.z; z++) {
        for (int y = 0; y < counts.y; y++) {
            for (int x = 0; x < counts.x; x++) {
                const auto index = static_cast<uint32_t>((z * counts.y + y) * counts.x + x);
                ProbeData& p = probes[index];
                p.pos = glm::vec4(origin + (glm::vec3(x, y, z) + 0.5f) * spacing, 0.0f);
                p.rotationSeed = glm::vec4(dist(rng), dist(rng), 0.0f, 0.0f);
            }
        }
    }

    void* data = nullptr;
    if (vkMapMemory(device->getDevice(), m_probeDataMemory, 0, size, 0, &data) != VK_SUCCESS)
        throw std::runtime_error("failed to map probe data buffer!");
    std::memcpy(data, probes.data(), static_cast<size_t>(size));
    vkUnmapMemory(device->getDevice(), m_probeDataMemory);
}

void ProbeVolume::createParamsBuffer()
{
    const VkDeviceSize size = sizeof(VolumeProbe);
    device->createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         m_paramsBuffer, m_paramsMemory);
    // Keep mapped: tuning knobs (setHysteresis etc.) write straight into it
    if (vkMapMemory(device->getDevice(), m_paramsMemory, 0, size, 0, &m_paramsMapped) != VK_SUCCESS)
        throw std::runtime_error("failed to map probe params buffer!");
    updateParams();
}

void ProbeVolume::updateParams()
{
    if (m_paramsMapped)
        std::memcpy(m_paramsMapped, &m_params, sizeof(VolumeProbe));
}

void ProbeVolume::cleanup()
{
    const VkDevice d = device ? device->getDevice() : VK_NULL_HANDLE;
    if (!d) return;

    if (m_paramsBuffer)  vkDestroyBuffer(d, m_paramsBuffer, nullptr);
    if (m_paramsMemory)  vkFreeMemory(d, m_paramsMemory, nullptr);
    if (m_probeDataBuffer) vkDestroyBuffer(d, m_probeDataBuffer, nullptr);
    if (m_probeDataMemory) vkFreeMemory(d, m_probeDataMemory, nullptr);
    if (m_sampler)       vkDestroySampler(d, m_sampler, nullptr);

    for (uint32_t i = 0; i < PING_PONG_COUNT; i++) {
        if (m_irradianceViews[i]) vkDestroyImageView(d, m_irradianceViews[i], nullptr);
        if (m_depthViews[i])      vkDestroyImageView(d, m_depthViews[i], nullptr);
        if (m_irradianceImages[i]) vkDestroyImage(d, m_irradianceImages[i], nullptr);
        if (m_depthImages[i])      vkDestroyImage(d, m_depthImages[i], nullptr);
        if (m_irradianceMemories[i]) vkFreeMemory(d, m_irradianceMemories[i], nullptr);
        if (m_depthMemories[i])      vkFreeMemory(d, m_depthMemories[i], nullptr);
    }

    m_irradianceViews.fill(VK_NULL_HANDLE);
    m_depthViews.fill(VK_NULL_HANDLE);
    m_irradianceImages.fill(VK_NULL_HANDLE);
    m_depthImages.fill(VK_NULL_HANDLE);
    m_irradianceMemories.fill(VK_NULL_HANDLE);
    m_depthMemories.fill(VK_NULL_HANDLE);
    m_sampler = VK_NULL_HANDLE;
    m_paramsBuffer = VK_NULL_HANDLE;
    m_paramsMemory = VK_NULL_HANDLE;
    m_paramsMapped = nullptr;
    m_probeDataBuffer = VK_NULL_HANDLE;
    m_probeDataMemory = VK_NULL_HANDLE;
}
