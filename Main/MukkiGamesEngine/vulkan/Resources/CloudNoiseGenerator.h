#pragma once
#include <vector>
#include <cstdint>

std::vector<uint8_t> generatePerlinWorley3D(int width, int height, int depth);
std::vector<uint8_t> generateWeatherMap2D(int width, int height, int channels =4);
