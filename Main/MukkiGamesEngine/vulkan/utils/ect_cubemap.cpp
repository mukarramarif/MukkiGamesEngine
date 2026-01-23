#include <iostream>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <gli/gli.hpp>
#include "utils.h" 
#include "ect_cubemap.h"
#include <cmath>
#include <stb_image_write.h>



// Vulkan cubemap face indices (standard order)
#define CUBE_MAP_INDEX_POS_X 0  // +X (right)
#define CUBE_MAP_INDEX_NEG_X 1  // -X (left)
#define CUBE_MAP_INDEX_POS_Y 2  // +Y (top)
#define CUBE_MAP_INDEX_NEG_Y 3  // -Y (bottom)
#define CUBE_MAP_INDEX_POS_Z 4  // +Z (front)
#define CUBE_MAP_INDEX_NEG_Z 5  // -Z (back)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

constexpr auto CUBE_NUM_FACES = 6;

// Convert 2D face coordinates to 3D direction vector for cubemap sampling
static glm::vec3 FaceCoordsToXYZ(int x, int y, int faceId, int faceSize) {
    // Map pixel coordinates to [-1, 1] range
    float u = (2.0f * (x + 0.5f) / faceSize) - 1.0f;
    float v = (2.0f * (y + 0.5f) / faceSize) - 1.0f;

    glm::vec3 dir;

    // Vulkan cubemap coordinate system
    switch (faceId) {
    case CUBE_MAP_INDEX_POS_X:  // +X
        dir = glm::vec3(1.0f, -v, -u);
        break;
    case CUBE_MAP_INDEX_NEG_X:  // -X
        dir = glm::vec3(-1.0f, -v, u);
        break;
    case CUBE_MAP_INDEX_POS_Y:  // +Y
        dir = glm::vec3(u, 1.0f, v);
        break;
    case CUBE_MAP_INDEX_NEG_Y:  // -Y
        dir = glm::vec3(u, -1.0f, -v);
        break;
    case CUBE_MAP_INDEX_POS_Z:  // +Z
        dir = glm::vec3(u, -v, 1.0f);
        break;
    case CUBE_MAP_INDEX_NEG_Z:  // -Z
        dir = glm::vec3(-u, -v, -1.0f);
        break;
    default:
        dir = glm::vec3(0.0f);
        break;
    }

    return dir;
}

int ConvertEctToCubemapFaces(const Bitmap& ectBitmap, std::vector<Bitmap>& outCubemapFaces)
{
    int faceSize = ectBitmap.w_ / 4;
    
    std::cout << "=== CUBEMAP DEBUG ===" << std::endl;
  /*  std::cout << "Source image: " << ectBitmap.w_ << "x" << ectBitmap.h_ << std::endl;
    std::cout << "Face size: " << faceSize << std::endl;*/

    outCubemapFaces.resize(CUBE_NUM_FACES);

    for (int i = 0; i < CUBE_NUM_FACES; i++) {
        outCubemapFaces[i].Init(faceSize, faceSize, ectBitmap.comp_, ectBitmap.fmt_);
    }

    int maxW = ectBitmap.w_ - 1;
    int maxH = ectBitmap.h_ - 1;

    const char* faceNames[] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };

    for (int face = 0; face < CUBE_NUM_FACES; face++) {
        // Debug: sample the center of each face
       /* glm::vec3 centerDir = glm::normalize(FaceCoordsToXYZ(faceSize/2, faceSize/2, face, faceSize));
        std::cout << "Face " << faceNames[face] << " center direction: (" 
                  << centerDir.x << ", " << centerDir.y << ", " << centerDir.z << ")" << std::endl;*/

        for (int y = 0; y < faceSize; y++) {
            for (int x = 0; x < faceSize; x++) {
                glm::vec3 dir = glm::normalize(FaceCoordsToXYZ(x, y, face, faceSize));
                
                float phi = atan2f(dir.z, dir.x);
                float theta = asinf(glm::clamp(dir.y, -1.0f, 1.0f));

                float u_tex = (phi + static_cast<float>(M_PI)) / (2.0f * static_cast<float>(M_PI));
                float v_tex = (static_cast<float>(M_PI) / 2.0f - theta) / static_cast<float>(M_PI);

                float U = u_tex * ectBitmap.w_;
                float V = v_tex * ectBitmap.h_;

                int U1 = CLAMP(static_cast<int>(floor(U)), 0, maxW);
                int V1 = CLAMP(static_cast<int>(floor(V)), 0, maxH);
                int U2 = CLAMP(U1 + 1, 0, maxW);
                int V2 = CLAMP(V1 + 1, 0, maxH);

                float s = U - U1;
                float t = V - V1;

                glm::vec4 A = ectBitmap.getPixel(U1, V1);
                glm::vec4 B = ectBitmap.getPixel(U2, V1);
                glm::vec4 C = ectBitmap.getPixel(U1, V2);
                glm::vec4 D = ectBitmap.getPixel(U2, V2);

                glm::vec4 color = A * (1 - s) * (1 - t) +
                                  B * s * (1 - t) +
                                  C * (1 - s) * t +
                                  D * s * t;

                outCubemapFaces[face].setPixel(x, y, color);
            }
        }

      
        
        std::vector<uint8_t> pixels(faceSize * faceSize * 4);
        for (int py = 0; py < faceSize; py++) {
            for (int px = 0; px < faceSize; px++) {
                glm::vec4 col = outCubemapFaces[face].getPixel(px, py);
                int idx = (py * faceSize + px) * 4;
                pixels[idx + 0] = static_cast<uint8_t>(glm::clamp(col.r * 255.0f, 0.0f, 255.0f));
                pixels[idx + 1] = static_cast<uint8_t>(glm::clamp(col.g * 255.0f, 0.0f, 255.0f));
                pixels[idx + 2] = static_cast<uint8_t>(glm::clamp(col.b * 255.0f, 0.0f, 255.0f));
                pixels[idx + 3] = static_cast<uint8_t>(glm::clamp(col.a * 255.0f, 0.0f, 255.0f));
            }
        }
      
    
    }

    // Also save source image for reference
    /*{
        std::vector<uint8_t> srcPixels(ectBitmap.w_ * ectBitmap.h_ * 4);
        for (int py = 0; py < ectBitmap.h_; py++) {
            for (int px = 0; px < ectBitmap.w_; px++) {
                glm::vec4 col = ectBitmap.getPixel(px, py);
                int idx = (py * ectBitmap.w_ + px) * 4;
                srcPixels[idx + 0] = static_cast<uint8_t>(glm::clamp(col.r * 255.0f, 0.0f, 255.0f));
                srcPixels[idx + 1] = static_cast<uint8_t>(glm::clamp(col.g * 255.0f, 0.0f, 255.0f));
                srcPixels[idx + 2] = static_cast<uint8_t>(glm::clamp(col.b * 255.0f, 0.0f, 255.0f));
                srcPixels[idx + 3] = static_cast<uint8_t>(glm::clamp(col.a * 255.0f, 0.0f, 255.0f));
            }
        }

    }*/

    std::cout << "=== END CUBEMAP DEBUG ===" << std::endl;

    return faceSize;
}
