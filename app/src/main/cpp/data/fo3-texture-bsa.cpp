#include "fo3-texture-bsa.h"
#include "fo3-asset-store.h"

#include <android/log.h>
#include <algorithm>
#include <cstring>
#include <utility>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr int MAX_TEXTURE_DIMENSION = 8192;

#define FQ_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define FQ_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define FQ_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

uint16_t ReadLe16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

uint32_t ReadLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

constexpr uint32_t FourCC(char a, char b, char c, char d) {
    return static_cast<uint32_t>(static_cast<uint8_t>(a)) |
           (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8) |
           (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24);
}

struct Rgba {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

Rgba Decode565(uint16_t value) {
    const uint8_t r5 = static_cast<uint8_t>((value >> 11) & 31u);
    const uint8_t g6 = static_cast<uint8_t>((value >> 5) & 63u);
    const uint8_t b5 = static_cast<uint8_t>(value & 31u);
    return {
        static_cast<uint8_t>((r5 * 255u + 15u) / 31u),
        static_cast<uint8_t>((g6 * 255u + 31u) / 63u),
        static_cast<uint8_t>((b5 * 255u + 15u) / 31u),
        255u,
    };
}

Rgba Mix(const Rgba& a, const Rgba& b, int wa, int wb, int divisor) {
    return {
        static_cast<uint8_t>((wa * a.r + wb * b.r) / divisor),
        static_cast<uint8_t>((wa * a.g + wb * b.g) / divisor),
        static_cast<uint8_t>((wa * a.b + wb * b.b) / divisor),
        255u,
    };
}

void BuildColorPalette(const uint8_t* block, Rgba palette[4], bool forceFourColor) {
    const uint16_t c0 = ReadLe16(block + 0);
    const uint16_t c1 = ReadLe16(block + 2);
    palette[0] = Decode565(c0);
    palette[1] = Decode565(c1);

    if (c0 > c1 || forceFourColor) {
        palette[2] = Mix(palette[0], palette[1], 2, 1, 3);
        palette[3] = Mix(palette[0], palette[1], 1, 2, 3);
    } else {
        palette[2] = Mix(palette[0], palette[1], 1, 1, 2);
        palette[3] = {0, 0, 0, 0};
    }
}

void StorePixel(std::vector<uint8_t>& rgba, int width, int height,
                int x, int y, const Rgba& pixel) {
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    const size_t dst = (static_cast<size_t>(y) * static_cast<size_t>(width) +
                        static_cast<size_t>(x)) * 4u;
    rgba[dst + 0] = pixel.r;
    rgba[dst + 1] = pixel.g;
    rgba[dst + 2] = pixel.b;
    rgba[dst + 3] = pixel.a;
}

bool DecodeDxt1(const uint8_t* src, size_t srcSize, int width, int height,
                std::vector<uint8_t>& rgba) {
    const int blocksX = (width + 3) / 4;
    const int blocksY = (height + 3) / 4;
    const size_t required = static_cast<size_t>(blocksX) * blocksY * 8u;
    if (srcSize < required) return false;

    rgba.assign(static_cast<size_t>(width) * height * 4u, 0u);
    size_t offset = 0;
    for (int by = 0; by < blocksY; ++by) {
        for (int bx = 0; bx < blocksX; ++bx) {
            const uint8_t* block = src + offset;
            offset += 8u;
            Rgba palette[4]{};
            BuildColorPalette(block, palette, false);
            const uint32_t codes = ReadLe32(block + 4);
            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    const int p = py * 4 + px;
                    const uint32_t index = (codes >> (2 * p)) & 3u;
                    StorePixel(rgba, width, height, bx * 4 + px, by * 4 + py,
                               palette[index]);
                }
            }
        }
    }
    return true;
}

bool DecodeDxt3(const uint8_t* src, size_t srcSize, int width, int height,
                std::vector<uint8_t>& rgba) {
    const int blocksX = (width + 3) / 4;
    const int blocksY = (height + 3) / 4;
    const size_t required = static_cast<size_t>(blocksX) * blocksY * 16u;
    if (srcSize < required) return false;

    rgba.assign(static_cast<size_t>(width) * height * 4u, 0u);
    size_t offset = 0;
    for (int by = 0; by < blocksY; ++by) {
        for (int bx = 0; bx < blocksX; ++bx) {
            const uint8_t* block = src + offset;
            offset += 16u;
            uint64_t alphaBits = 0;
            for (int i = 0; i < 8; ++i) {
                alphaBits |= static_cast<uint64_t>(block[i]) << (8 * i);
            }
            Rgba palette[4]{};
            BuildColorPalette(block + 8, palette, true);
            const uint32_t codes = ReadLe32(block + 12);
            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    const int p = py * 4 + px;
                    const uint32_t index = (codes >> (2 * p)) & 3u;
                    Rgba pixel = palette[index];
                    const uint8_t a4 = static_cast<uint8_t>((alphaBits >> (4 * p)) & 0xFu);
                    pixel.a = static_cast<uint8_t>(a4 * 17u);
                    StorePixel(rgba, width, height, bx * 4 + px, by * 4 + py, pixel);
                }
            }
        }
    }
    return true;
}

bool DecodeDxt5(const uint8_t* src, size_t srcSize, int width, int height,
                std::vector<uint8_t>& rgba) {
    const int blocksX = (width + 3) / 4;
    const int blocksY = (height + 3) / 4;
    const size_t required = static_cast<size_t>(blocksX) * blocksY * 16u;
    if (srcSize < required) return false;

    rgba.assign(static_cast<size_t>(width) * height * 4u, 0u);
    size_t offset = 0;
    for (int by = 0; by < blocksY; ++by) {
        for (int bx = 0; bx < blocksX; ++bx) {
            const uint8_t* block = src + offset;
            offset += 16u;

            const uint8_t a0 = block[0];
            const uint8_t a1 = block[1];
            uint8_t alpha[8]{};
            alpha[0] = a0;
            alpha[1] = a1;
            if (a0 > a1) {
                for (int i = 1; i <= 6; ++i) {
                    alpha[i + 1] = static_cast<uint8_t>(
                            ((7 - i) * static_cast<int>(a0) + i * static_cast<int>(a1)) / 7);
                }
            } else {
                for (int i = 1; i <= 4; ++i) {
                    alpha[i + 1] = static_cast<uint8_t>(
                            ((5 - i) * static_cast<int>(a0) + i * static_cast<int>(a1)) / 5);
                }
                alpha[6] = 0;
                alpha[7] = 255;
            }

            uint64_t alphaCodes = 0;
            for (int i = 0; i < 6; ++i) {
                alphaCodes |= static_cast<uint64_t>(block[2 + i]) << (8 * i);
            }

            Rgba palette[4]{};
            BuildColorPalette(block + 8, palette, true);
            const uint32_t colorCodes = ReadLe32(block + 12);
            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    const int p = py * 4 + px;
                    const uint32_t colorIndex = (colorCodes >> (2 * p)) & 3u;
                    const uint32_t alphaIndex = static_cast<uint32_t>((alphaCodes >> (3 * p)) & 7u);
                    Rgba pixel = palette[colorIndex];
                    pixel.a = alpha[alphaIndex];
                    StorePixel(rgba, width, height, bx * 4 + px, by * 4 + py, pixel);
                }
            }
        }
    }
    return true;
}

uint8_t ExpandMasked(uint32_t pixel, uint32_t mask, uint8_t fallback) {
    if (mask == 0u) return fallback;
    int shift = 0;
    while (shift < 32 && ((mask >> shift) & 1u) == 0u) ++shift;
    const uint32_t normalizedMask = mask >> shift;
    const uint32_t value = (pixel & mask) >> shift;
    if (normalizedMask == 0u) return fallback;
    return static_cast<uint8_t>((static_cast<uint64_t>(value) * 255u +
                                 normalizedMask / 2u) / normalizedMask);
}

bool DecodeRgb(const uint8_t* src, size_t srcSize, int width, int height,
               uint32_t pitch, uint32_t bitsPerPixel,
               uint32_t rMask, uint32_t gMask, uint32_t bMask, uint32_t aMask,
               std::vector<uint8_t>& rgba) {
    if (bitsPerPixel != 24u && bitsPerPixel != 32u) return false;
    const size_t bytesPerPixel = bitsPerPixel / 8u;
    const size_t minimumPitch = static_cast<size_t>(width) * bytesPerPixel;
    size_t rowPitch = pitch >= minimumPitch ? pitch : minimumPitch;
    if (rowPitch * static_cast<size_t>(height) > srcSize) {
        rowPitch = minimumPitch;
    }
    if (rowPitch * static_cast<size_t>(height) > srcSize) return false;

    rgba.resize(static_cast<size_t>(width) * height * 4u);
    for (int y = 0; y < height; ++y) {
        const uint8_t* row = src + static_cast<size_t>(y) * rowPitch;
        for (int x = 0; x < width; ++x) {
            const uint8_t* p = row + static_cast<size_t>(x) * bytesPerPixel;
            uint32_t pixel = static_cast<uint32_t>(p[0]) |
                             (static_cast<uint32_t>(p[1]) << 8) |
                             (static_cast<uint32_t>(p[2]) << 16);
            if (bytesPerPixel == 4u) pixel |= static_cast<uint32_t>(p[3]) << 24;
            const size_t dst = (static_cast<size_t>(y) * width + x) * 4u;
            rgba[dst + 0] = ExpandMasked(pixel, rMask, 0);
            rgba[dst + 1] = ExpandMasked(pixel, gMask, 0);
            rgba[dst + 2] = ExpandMasked(pixel, bMask, 0);
            rgba[dst + 3] = ExpandMasked(pixel, aMask, 255);
        }
    }
    return true;
}

bool DecodeDds(const std::vector<uint8_t>& dds, Fo3RgbaTexture& out) {
    if (dds.size() < 128u || std::memcmp(dds.data(), "DDS ", 4) != 0) return false;
    if (ReadLe32(dds.data() + 4) != 124u || ReadLe32(dds.data() + 76) != 32u) return false;

    const uint32_t height = ReadLe32(dds.data() + 12);
    const uint32_t width = ReadLe32(dds.data() + 16);
    const uint32_t pitch = ReadLe32(dds.data() + 20);
    const uint32_t pixelFlags = ReadLe32(dds.data() + 80);
    const uint32_t fourCC = ReadLe32(dds.data() + 84);
    const uint32_t bitsPerPixel = ReadLe32(dds.data() + 88);
    const uint32_t rMask = ReadLe32(dds.data() + 92);
    const uint32_t gMask = ReadLe32(dds.data() + 96);
    const uint32_t bMask = ReadLe32(dds.data() + 100);
    const uint32_t aMask = ReadLe32(dds.data() + 104);

    if (width == 0 || height == 0 || width > MAX_TEXTURE_DIMENSION ||
        height > MAX_TEXTURE_DIMENSION) return false;

    const uint8_t* pixels = dds.data() + 128u;
    const size_t pixelBytes = dds.size() - 128u;
    std::vector<uint8_t> rgba;
    std::string format;
    bool ok = false;

    if ((pixelFlags & 0x4u) != 0u) { // DDPF_FOURCC
        if (fourCC == FourCC('D', 'X', 'T', '1')) {
            format = "DXT1/BC1";
            ok = DecodeDxt1(pixels, pixelBytes, static_cast<int>(width),
                            static_cast<int>(height), rgba);
        } else if (fourCC == FourCC('D', 'X', 'T', '3')) {
            format = "DXT3/BC2";
            ok = DecodeDxt3(pixels, pixelBytes, static_cast<int>(width),
                            static_cast<int>(height), rgba);
        } else if (fourCC == FourCC('D', 'X', 'T', '5')) {
            format = "DXT5/BC3";
            ok = DecodeDxt5(pixels, pixelBytes, static_cast<int>(width),
                            static_cast<int>(height), rgba);
        } else if (fourCC == FourCC('D', 'X', '1', '0')) {
            FQ_LOGE("Q5H DDS DX10 header is not expected for vanilla Fallout 3 texture");
            return false;
        } else {
            char code[5]{
                static_cast<char>(fourCC & 0xffu),
                static_cast<char>((fourCC >> 8) & 0xffu),
                static_cast<char>((fourCC >> 16) & 0xffu),
                static_cast<char>((fourCC >> 24) & 0xffu),
                0,
            };
            FQ_LOGE("Q5H unsupported DDS FourCC: %s", code);
            return false;
        }
    } else if ((pixelFlags & 0x40u) != 0u) { // DDPF_RGB
        format = bitsPerPixel == 32u ? "RGBA32" : "RGB24";
        ok = DecodeRgb(pixels, pixelBytes, static_cast<int>(width),
                       static_cast<int>(height), pitch, bitsPerPixel,
                       rMask, gMask, bMask, aMask, rgba);
    }

    if (!ok) return false;
    out.width = static_cast<int>(width);
    out.height = static_cast<int>(height);
    out.rgba = std::move(rgba);
    out.format = format;
    return true;
}


size_t DdsMipByteSizeQ2050(uint32_t pixelFlags, uint32_t fourCC,
                           uint32_t bitsPerPixel, int width, int height) {
    const int w = std::max(width, 1);
    const int h = std::max(height, 1);
    if ((pixelFlags & 0x4u) != 0u) {
        const size_t blocksX = static_cast<size_t>((w + 3) / 4);
        const size_t blocksY = static_cast<size_t>((h + 3) / 4);
        if (fourCC == FourCC('D', 'X', 'T', '1')) return blocksX * blocksY * 8u;
        if (fourCC == FourCC('D', 'X', 'T', '3') ||
            fourCC == FourCC('D', 'X', 'T', '5')) return blocksX * blocksY * 16u;
        return 0u;
    }
    if ((pixelFlags & 0x40u) != 0u &&
        (bitsPerPixel == 24u || bitsPerPixel == 32u)) {
        return static_cast<size_t>(w) * static_cast<size_t>(h) *
               static_cast<size_t>(bitsPerPixel / 8u);
    }
    return 0u;
}

bool DecodeDdsLevelQ2050(const uint8_t* pixels, size_t pixelBytes,
                         uint32_t pixelFlags, uint32_t fourCC,
                         uint32_t bitsPerPixel,
                         uint32_t rMask, uint32_t gMask,
                         uint32_t bMask, uint32_t aMask,
                         int width, int height,
                         std::vector<uint8_t>& rgba,
                         std::string& format) {
    if ((pixelFlags & 0x4u) != 0u) {
        if (fourCC == FourCC('D', 'X', 'T', '1')) {
            format = "DXT1/BC1";
            return DecodeDxt1(pixels, pixelBytes, width, height, rgba);
        }
        if (fourCC == FourCC('D', 'X', 'T', '3')) {
            format = "DXT3/BC2";
            return DecodeDxt3(pixels, pixelBytes, width, height, rgba);
        }
        if (fourCC == FourCC('D', 'X', 'T', '5')) {
            format = "DXT5/BC3";
            return DecodeDxt5(pixels, pixelBytes, width, height, rgba);
        }
        return false;
    }
    if ((pixelFlags & 0x40u) != 0u) {
        format = bitsPerPixel == 32u ? "RGBA32" : "RGB24";
        const uint32_t rowPitch =
            static_cast<uint32_t>(std::max(width, 1)) * (bitsPerPixel / 8u);
        return DecodeRgb(pixels, pixelBytes, width, height, rowPitch,
                         bitsPerPixel, rMask, gMask, bMask, aMask, rgba);
    }
    return false;
}

bool DecodeDdsCubeQ2050(const std::vector<uint8_t>& dds,
                        Fo3RgbaCubeTexture& out) {
    if (dds.size() < 128u || std::memcmp(dds.data(), "DDS ", 4) != 0) return false;
    if (ReadLe32(dds.data() + 4) != 124u || ReadLe32(dds.data() + 76) != 32u) return false;

    const uint32_t height = ReadLe32(dds.data() + 12);
    const uint32_t width = ReadLe32(dds.data() + 16);
    uint32_t mipLevels = ReadLe32(dds.data() + 28);
    if (mipLevels == 0u) mipLevels = 1u;
    const uint32_t pixelFlags = ReadLe32(dds.data() + 80);
    const uint32_t fourCC = ReadLe32(dds.data() + 84);
    const uint32_t bitsPerPixel = ReadLe32(dds.data() + 88);
    const uint32_t rMask = ReadLe32(dds.data() + 92);
    const uint32_t gMask = ReadLe32(dds.data() + 96);
    const uint32_t bMask = ReadLe32(dds.data() + 100);
    const uint32_t aMask = ReadLe32(dds.data() + 104);
    const uint32_t caps2 = ReadLe32(dds.data() + 112);

    constexpr uint32_t DDSCAPS2_CUBEMAP_Q2050 = 0x00000200u;
    constexpr uint32_t DDSCAPS2_ALL_FACES_Q2050 =
        0x00000400u | 0x00000800u | 0x00001000u |
        0x00002000u | 0x00004000u | 0x00008000u;
    if ((caps2 & DDSCAPS2_CUBEMAP_Q2050) == 0u ||
        (caps2 & DDSCAPS2_ALL_FACES_Q2050) != DDSCAPS2_ALL_FACES_Q2050) {
        FQ_LOGE("Q20.5 DDS is not a complete legacy cubemap caps2=0x%08X", caps2);
        return false;
    }
    if (width == 0u || height == 0u ||
        width > static_cast<uint32_t>(MAX_TEXTURE_DIMENSION) ||
        height > static_cast<uint32_t>(MAX_TEXTURE_DIMENSION) ||
        mipLevels > 16u) return false;

    size_t offset = 128u;
    std::string format;
    for (size_t face = 0u; face < 6u; ++face) {
        out.rgbaLevels[face].clear();
        out.rgbaLevels[face].reserve(mipLevels);
        int mipWidth = static_cast<int>(width);
        int mipHeight = static_cast<int>(height);
        for (uint32_t mip = 0u; mip < mipLevels; ++mip) {
            const size_t levelBytes =
                DdsMipByteSizeQ2050(pixelFlags, fourCC, bitsPerPixel,
                                    mipWidth, mipHeight);
            if (levelBytes == 0u || offset > dds.size() ||
                levelBytes > dds.size() - offset) return false;

            std::vector<uint8_t> rgba;
            std::string levelFormat;
            if (!DecodeDdsLevelQ2050(dds.data() + offset, levelBytes,
                                     pixelFlags, fourCC, bitsPerPixel,
                                     rMask, gMask, bMask, aMask,
                                     mipWidth, mipHeight, rgba, levelFormat)) {
                return false;
            }
            if (format.empty()) format = levelFormat;
            out.rgbaLevels[face].push_back(std::move(rgba));
            offset += levelBytes;
            mipWidth = std::max(1, mipWidth >> 1);
            mipHeight = std::max(1, mipHeight >> 1);
        }
    }

    out.width = static_cast<int>(width);
    out.height = static_cast<int>(height);
    out.mipLevels = static_cast<int>(mipLevels);
    out.format = format;
    return true;
}

bool TryArchiveCubeQ2050(const char* archivePath,
                         const std::string& texturePath,
                         Fo3RgbaCubeTexture& outTexture) {
    std::vector<uint8_t> dds;
    fo3assets::BsaFileInfo info;
    if (!fo3assets::GetBsaArchive(archivePath)->Read(
            texturePath, dds, &info, fo3assets::BsaPathKind::Texture)) return false;

    if (!DecodeDdsCubeQ2050(dds, outTexture)) {
        FQ_LOGE("Q20.5 CUBE DDS decode failed: %s bytes=%zu",
                info.path.c_str(), dds.size());
        return false;
    }
    outTexture.sourcePath = info.path;
    FQ_LOGI("Q20.5 CUBE DDS READY: %dx%d mips=%d format=%s path=%s compressed=%d",
            outTexture.width, outTexture.height, outTexture.mipLevels,
            outTexture.format.c_str(), outTexture.sourcePath.c_str(),
            info.compressed ? 1 : 0);
    return true;
}

bool TryArchive(const char* archivePath, const std::string& texturePath,
                Fo3RgbaTexture& outTexture) {
    std::vector<uint8_t> dds;
    fo3assets::BsaFileInfo info;
    if (!fo3assets::GetBsaArchive(archivePath)->Read(
            texturePath, dds, &info, fo3assets::BsaPathKind::Texture)) {
        FQ_LOGW("Q5H texture not found or extraction failed in %s: %s",
                archivePath, texturePath.c_str());
        return false;
    }
    FQ_LOGI("Q5H TEXTURE FOUND: path=%s storedBytes=%u decodedArchiveBytes=%zu compressed=%d",
            info.path.c_str(), info.storedBytes, dds.size(), info.compressed ? 1 : 0);

    if (!DecodeDds(dds, outTexture)) {
        FQ_LOGE("Q5H DDS decode failed: %s bytes=%zu",
                info.path.c_str(), dds.size());
        return false;
    }

    outTexture.sourcePath = info.path;
    FQ_LOGI("Q5H DDS READY: %dx%d format=%s rgbaBytes=%zu path=%s",
            outTexture.width, outTexture.height, outTexture.format.c_str(),
            outTexture.rgba.size(), outTexture.sourcePath.c_str());
    return true;
}

} // namespace

bool LoadFalloutTextureRgba(const std::string& texturePath, Fo3RgbaTexture& outTexture) {
    outTexture = {};
    if (texturePath.empty()) return false;

    for (const std::string& archivePath : fo3assets::TextureArchivePaths()) {
        if (TryArchive(archivePath.c_str(), texturePath, outTexture)) return true;
    }

    FQ_LOGE("Q5H FAILED: diffuse texture could not be loaded from either texture BSA name: %s",
            texturePath.c_str());
    return false;
}

bool LoadFalloutCubeTextureRgba(const std::string& texturePath,
                                 Fo3RgbaCubeTexture& outTexture) {
    outTexture = {};
    if (texturePath.empty()) return false;

    for (const std::string& archivePath : fo3assets::TextureArchivePaths()) {
        if (TryArchiveCubeQ2050(archivePath.c_str(), texturePath, outTexture)) return true;
    }

    FQ_LOGE("Q20.5 CUBE FAILED: texture could not be loaded from either texture BSA name: %s",
            texturePath.c_str());
    return false;
}
