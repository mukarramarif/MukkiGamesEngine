#pragma once
#include <vector>
#include <cstdint>

struct CloudNoiseParams {
    // ── Perlin-Worley 3D ──────────────────────────────────────
    float perlinWorleyScale = 4.0f;   // frequency scale divisor
    int   perlinOctaves     = 8;      // Perlin FBM octaves
    int   worleyOctaves     = 6;      // Worley FBM octaves

    // ── Weather Map 2D ───────────────────────────────────────
    float weatherScale = 3.0f;        // frequency scale divisor

    // R channel: cloud coverage (0 = clear, 1 = overcast)
    int   coverageOctaves    = 3;
    float coverageMultiplier = 1.2f;
    float coverageOffset     = -0.1f;
    float coveragePower      = 0.7f;

    // G channel: precipitation chance (0 = dry, 1 = raining)
    int   precipOctaves       = 4;
    float precipFreqMultiplier = 2.0f;
    float precipMultiplier    = 1.5f;
    float precipOffset        = -0.3f;
    float precipPower         = 1.5f;

    // B channel: cloud type (0 = stratus, 1 = cumulus)
    int   cloudTypeOctaves       = 3;
    float cloudTypeFreqMultiplier = 1.5f;
    float cloudTypeOffset        = 5.0f;
    float cloudTypeMultiplier    = 1.0f;
};

// Default parameters — uses CloudNoiseParams{} (all defaults)
std::vector<uint8_t> generatePerlinWorley3D(int width, int height, int depth,
                                            const CloudNoiseParams& params = CloudNoiseParams{});
std::vector<uint8_t> generateWeatherMap2D(int width, int height, int channels,
                                          const CloudNoiseParams& params = CloudNoiseParams{});
