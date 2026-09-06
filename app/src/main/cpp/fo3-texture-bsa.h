#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Fo3RgbaTexture {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
    std::string sourcePath;
    std::string format;
};

bool LoadFalloutTextureRgba(const std::string& texturePath, Fo3RgbaTexture& outTexture);
