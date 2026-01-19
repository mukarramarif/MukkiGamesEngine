#pragma once
#include <vector>
#include "../objects/bitmap.h"

int ConvertEctToCubemapFaces(const Bitmap& ectBitmap, std::vector<Bitmap>& outCubemapFaces);