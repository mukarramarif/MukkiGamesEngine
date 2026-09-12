#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <vector>
#include <glm/glm.hpp>
#include "../Core/VkDevice.h"

struct VolumeProbe{
   glm::vec4 origin;
   glm::ivec4 probeCounts; // xyz = probes per axis, w = total probe count
   glm::vec4 params; // x = probeSpacing, y = maxRayDistance, z = hysteresis, w = normalBias
   glm::vec4 atlas; // x = tilesPerSide, y = irradianceTexels, z = depthTexels, w = raysPerProbe
   // Debug/tuning: x = raster debug mode (0 off, 1 GI only, 2 GI heatmap,
   // 3 probe cells), y = GI strength, z = relocation enabled,
   // w = feedback gain.
   // Must match the mirrors in probeTrace.rgen and brdf.slang (std140).
   glm::vec4 debug;
   // Shadow information: x = probe shadow strength in the raster (0 = off,
   // 1 = full), y = penumbra softening, zw = unused. The per-direction
   // visibility itself lives in the irradiance atlas alpha channel.
   glm::vec4 shadow;
};


struct ProbeData{
    glm::vec4 pos;
    glm::vec4 rotationSeed;
};
class ProbeVolume {

public:
    static constexpr uint32_t DEFAULT_IRRADIANCE_TEXELS = 8;
    static constexpr uint32_t DEFAULT_DEPTH_TEXELS      = 16;
    static constexpr uint32_t PING_PONG_COUNT           = 2;
    static constexpr uint32_t DEFAULT_RAYS_PER_PROBE    = 256;

    ProbeVolume() = default;
    ~ProbeVolume();
    ProbeVolume(const ProbeVolume&) = delete;
    ProbeVolume& operator=(const ProbeVolume&) = delete;

    bool init(Device* device,
              const glm::ivec3& probeCounts,
              float probeSpacing,
              const glm::vec3& origin,
              uint32_t irradianceTexelsPerProbe = DEFAULT_IRRADIANCE_TEXELS,
              uint32_t depthTexelsPerProbe      = DEFAULT_DEPTH_TEXELS);
    void cleanup();
     // Derive a grid covering a scene AABB (helper for loadSceneObjects()).
    static void volumeFromAABB(const glm::vec3& sceneMin, const glm::vec3& sceneMax,
                               float probeSpacing,
                               glm::ivec3& outCounts, glm::vec3& outOrigin);

    // Tuning knobs - update the mapped UBO immediately (host-coherent).
    void setHysteresis(float hysteresis);
    void setNormalBias(float normalBias);
    void setMaxRayDistance(float maxRayDistance);
    void setDebugMode(int mode);
    void setGIStrength(float strength);
    void setRelocationEnabled(bool enabled);
    void setFeedbackGain(float gain);
    void setProbeShadowStrength(float strength);

    // Debug: read the (possibly GPU-relocated) probe positions back from the
    // host-visible probe data buffer. Unsynchronized (torn reads possible) -
    // debug display only.
    bool getProbePositionsCPU(std::vector<glm::vec4>& outPositions) const;
    // Grid position a probe would occupy without relocation.
    glm::vec3 gridPositionForIndex(uint32_t index) const;

    const VolumeProbe& getParams() const { return m_params; }
    uint32_t getProbeCount()   const { return m_probeCount; }
    uint32_t getTilesPerSide() const { return m_tilesPerSide; }
    uint32_t getIrradianceAtlasSize() const { return m_tilesPerSide * m_irradianceTexels; }
    uint32_t getDepthAtlasSize()      const { return m_tilesPerSide * m_depthTexels; }

    // Ping-pong texture access (read one copy, write the other, swap each frame)
    VkImageView getIrradianceView(uint32_t index) const;
    VkImageView getDepthView(uint32_t index) const;
    VkImage     getIrradianceImage(uint32_t index) const;
    VkImage     getDepthImage(uint32_t index) const;
    VkSampler   getSampler() const { return m_sampler; }

    VkBuffer getProbeDataBuffer() const { return m_probeDataBuffer; }
    VkBuffer getParamsBuffer()   const { return m_paramsBuffer; }

    // Clears both ping-pong copies. Caller must already have the atlases in
    // VK_IMAGE_LAYOUT_GENERAL (this only records vkCmdClearColorImage).
    void clear(VkCommandBuffer cmd);

private:
    Device* device = nullptr;

    VolumeProbe m_params{};
    uint32_t m_probeCount       = 0;
    uint32_t m_tilesPerSide     = 0;
    uint32_t m_irradianceTexels = DEFAULT_IRRADIANCE_TEXELS;
    uint32_t m_depthTexels      = DEFAULT_DEPTH_TEXELS;

    // Ping-pong probe textures (feedback reads the previous frame's copy)
    std::array<VkImage, PING_PONG_COUNT>        m_irradianceImages{};
    std::array<VkDeviceMemory, PING_PONG_COUNT> m_irradianceMemories{};
    std::array<VkImageView, PING_PONG_COUNT>    m_irradianceViews{};
    std::array<VkImage, PING_PONG_COUNT>        m_depthImages{};
    std::array<VkDeviceMemory, PING_PONG_COUNT> m_depthMemories{};
    std::array<VkImageView, PING_PONG_COUNT>    m_depthViews{};
    VkSampler m_sampler = VK_NULL_HANDLE;

    // Per-probe data (SSBO) + volume params (UBO)
    VkBuffer       m_probeDataBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_probeDataMemory = VK_NULL_HANDLE;
    VkBuffer       m_paramsBuffer    = VK_NULL_HANDLE;
    VkDeviceMemory m_paramsMemory    = VK_NULL_HANDLE;
    void*          m_paramsMapped    = nullptr;

    void createAtlasImages(VkFormat format, uint32_t texelsPerProbe,
                           std::array<VkImage, PING_PONG_COUNT>& images,
                           std::array<VkDeviceMemory, PING_PONG_COUNT>& memories,
                           std::array<VkImageView, PING_PONG_COUNT>& views);
    void createSampler();
    void createProbeDataBuffer();
    void createParamsBuffer();
    void updateParams();
};
