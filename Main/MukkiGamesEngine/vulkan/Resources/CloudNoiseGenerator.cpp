#include "CloudNoiseGenerator.h"
#include <cmath>
#include <algorithm>
#include <random>

namespace {

uint8_t floatToByte(float v) {
    return static_cast<uint8_t>(std::clamp(static_cast<int>(v * 255.0f + 0.5f), 0, 255));
}

float hash(int x, int y, int z) {
    int h = x * 374761393 + y * 668265263 + z * 1274126177;
    h = (h ^ (h >> 13)) * 1274126177;
    h = h ^ (h >> 16);
    return static_cast<float>(h & 0x7fffffff) / 2147483648.0f;
}

float smoothstep01(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float perlin3D(float x, float y, float z) {
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    int zi = static_cast<int>(std::floor(z));
    float xf = x - static_cast<float>(xi);
    float yf = y - static_cast<float>(yi);
    float zf = z - static_cast<float>(zi);

    float u = smoothstep01(xf);
    float v = smoothstep01(yf);
    float w = smoothstep01(zf);

    float n000 = hash(xi,     yi,     zi);
    float n100 = hash(xi + 1, yi,     zi);
    float n010 = hash(xi,     yi + 1, zi);
    float n110 = hash(xi + 1, yi + 1, zi);
    float n001 = hash(xi,     yi,     zi + 1);
    float n101 = hash(xi + 1, yi,     zi + 1);
    float n011 = hash(xi,     yi + 1, zi + 1);
    float n111 = hash(xi + 1, yi + 1, zi + 1);

    float nx00 = lerp(n000, n100, u);
    float nx10 = lerp(n010, n110, u);
    float nx01 = lerp(n001, n101, u);
    float nx11 = lerp(n011, n111, u);
    float nxy0 = lerp(nx00, nx10, v);
    float nxy1 = lerp(nx01, nx11, v);
    return lerp(nxy0, nxy1, w);
}

float worley3D(float x, float y, float z) {
    int cx = static_cast<int>(std::floor(x));
    int cy = static_cast<int>(std::floor(y));
    int cz = static_cast<int>(std::floor(z));
    float fx = x - static_cast<float>(cx);
    float fy = y - static_cast<float>(cy);
    float fz = z - static_cast<float>(cz);

    float minDist = 100.0f;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dz = -1; dz <= 1; dz++) {
                int gx = cx + dx, gy = cy + dy, gz = cz + dz;
                float px = static_cast<float>(dx) + hash(gx, gy, gz) - fx;
                float py = static_cast<float>(dy) + hash(gx + 100, gy + 100, gz + 100) - fy;
                float pz = static_cast<float>(dz) + hash(gx + 200, gy + 200, gz + 200) - fz;
                float d = std::sqrt(px * px + py * py + pz * pz);
                if (d < minDist) minDist = d;
            }
        }
    }
    return std::clamp(minDist, 0.0f, 1.0f);
}

float fbm3D(float x, float y, float z, int octaves) {
    float value = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    for (int i = 0; i < octaves; i++) {
        value += amp * perlin3D(x * freq, y * freq, z * freq);
        freq *= 2.0f;
        amp *= 0.5f;
    }
    return value;
}

float fbmWorley3D(float x, float y, float z, int octaves) {
    float value = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    for (int i = 0; i < octaves; i++) {
        value += amp * worley3D(x * freq, y * freq, z * freq);
        freq *= 2.0f;
        amp *= 0.5f;
    }
    return value;
}

}

std::vector<uint8_t> generatePerlinWorley3D(int width, int height, int depth) {
    std::vector<uint8_t> data(width * height * depth * 2);
    const int perlinOctaves = 8;
    const int worleyOctaves = 6;

    float scaleX = 4.0f / static_cast<float>(width);
    float scaleY = 4.0f / static_cast<float>(height);
    float scaleZ = 4.0f / static_cast<float>(depth);

    for (int z = 0; z < depth; z++) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float px = static_cast<float>(x) * scaleX;
                float py = static_cast<float>(y) * scaleY;
                float pz = static_cast<float>(z) * scaleZ;

                float p = fbm3D(px, py, pz, perlinOctaves);
                float w = fbmWorley3D(px, py, pz, worleyOctaves);

                size_t idx = (z * height * width + y * width + x) * 2;
                data[idx + 0] = floatToByte(p);
                data[idx + 1] = floatToByte(w);
            }
        }
    }
    return data;
}

std::vector<uint8_t> generateWeatherMap2D(int width, int height, int channels) {
    std::vector<uint8_t> data(width * height * channels);
    std::mt19937 rng(42);

    float scaleX = 3.0f / static_cast<float>(width);
    float scaleY = 3.0f / static_cast<float>(height);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float px = static_cast<float>(x) * scaleX;
            float py = static_cast<float>(y) * scaleY;

            // --- R: Cloud coverage (0=clear, 1=overcast) ---
            float coverage = fbm3D(px, py, 0.0f, 3);
            coverage = std::clamp(coverage * 1.2f - 0.1f, 0.0f, 1.0f);
            coverage = std::pow(coverage, 0.7f);

            // --- G: Precipitation chance (0=dry, 1=raining) ---
            // Higher-frequency, sparser — rain is localised
            float precip = fbm3D(px * 2.0f, py * 2.0f, 1.0f, 4);
            precip = std::clamp(precip * 1.5f - 0.3f, 0.0f, 1.0f);
            precip *= coverage;                  // rain only where there are clouds
            precip = std::pow(precip, 1.5f);     // sparse rain cells

            // --- B: Cloud type (0=stratus, 0.5=stratocumulus, 1=cumulus) ---
            // Independent noise field so types vary across the map
            float cloudType = fbm3D(px * 1.5f + 5.0f, py * 1.5f + 5.0f, 2.0f, 3);
            cloudType = std::clamp(cloudType * 1.0f + 0.5f, 0.0f, 1.0f);

            // --- A: unused (set to 1.0) ---
            size_t idx = (y * width + x) * channels;
            data[idx + 0] = floatToByte(coverage);
            data[idx + 1] = floatToByte(precip);
            data[idx + 2] = floatToByte(cloudType);
            if (channels >= 4)
                data[idx + 3] = 255;  // alpha = 1.0
        }
    }
    return data;
}
