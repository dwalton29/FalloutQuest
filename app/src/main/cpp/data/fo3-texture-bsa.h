#pragma once

#include <array>
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

struct Fo3RgbaCubeTexture {
    int width = 0;
    int height = 0;
    int mipLevels = 0;
    // [face][mip], face order is +X,-X,+Y,-Y,+Z,-Z as authored by legacy DDS.
    std::array<std::vector<std::vector<uint8_t>>, 6> rgbaLevels;
    std::string sourcePath;
    std::string format;
};

bool LoadFalloutTextureRgba(const std::string& texturePath, Fo3RgbaTexture& outTexture);
bool LoadFalloutCubeTextureRgba(const std::string& texturePath, Fo3RgbaCubeTexture& outTexture);
