#include "fo3-texture-bsa.h"

#include <android/log.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <zlib.h>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* TEXTURE_BSA_PATHS[] = {
    "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout - Textures.bsa",
    "/data/user/0/com.falloutquest.app/files/Fallout3/Data/textures.bsa",
};
constexpr uint32_t BSA_VERSION_FO3 = 104u;
constexpr uint32_t MAX_TARGET_BYTES = 128u * 1024u * 1024u;
constexpr int MAX_TEXTURE_DIMENSION = 8192;

#define FQ_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define FQ_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define FQ_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

struct FolderRecord {
    uint32_t count = 0;
};

struct RawFileRecord {
    std::string folder;
    uint32_t size = 0;
    uint32_t offset = 0;
    bool compressionToggle = false;
};

struct BsaHeader {
    uint32_t version = 0;
    uint32_t foldersOffset = 0;
    uint32_t archiveFlags = 0;
    uint32_t folderCount = 0;
    uint32_t fileCount = 0;
    uint32_t totalFolderNameLength = 0;
    uint32_t totalFileNameLength = 0;
    uint32_t fileFlags = 0;
};

struct TargetEntry {
    bool found = false;
    std::string storedPath;
    uint32_t size = 0;
    uint32_t offset = 0;
    bool compressionToggle = false;
};

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

bool ReadExact(FILE* file, void* dst, size_t size) {
    return std::fread(dst, 1, size, file) == size;
}

std::string NormalizePath(std::string value) {
    for (char& ch : value) {
        if (ch == '/') ch = '\\';
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    while (!value.empty() && (value.front() == '\\' || value.front() == '/')) {
        value.erase(value.begin());
    }
    return value;
}

std::string WithoutTexturesPrefix(const std::string& value) {
    const std::string prefix = "textures\\";
    if (value.rfind(prefix, 0) == 0) return value.substr(prefix.size());
    return value;
}

bool ReadCString(FILE* file, std::string& out) {
    out.clear();
    for (size_t i = 0; i < 8192u; ++i) {
        const int ch = std::fgetc(file);
        if (ch == EOF) return false;
        if (ch == 0) return true;
        out.push_back(static_cast<char>(ch));
    }
    return false;
}

bool ReadHeader(FILE* file, BsaHeader& out) {
    uint8_t header[36]{};
    if (!ReadExact(file, header, sizeof(header))) return false;
    if (std::memcmp(header, "BSA\0", 4) != 0) return false;

    out.version = ReadLe32(header + 4);
    out.foldersOffset = ReadLe32(header + 8);
    out.archiveFlags = ReadLe32(header + 12);
    out.folderCount = ReadLe32(header + 16);
    out.fileCount = ReadLe32(header + 20);
    out.totalFolderNameLength = ReadLe32(header + 24);
    out.totalFileNameLength = ReadLe32(header + 28);
    out.fileFlags = ReadLe32(header + 32);
    return true;
}

bool FindTarget(FILE* file, const BsaHeader& header, const std::string& requestedPath,
                TargetEntry& target) {
    if (header.version != BSA_VERSION_FO3) return false;
    if ((header.archiveFlags & 1u) == 0 || (header.archiveFlags & 2u) == 0) return false;
    if (header.folderCount == 0 || header.folderCount > 1000000u ||
        header.fileCount == 0 || header.fileCount > 3000000u ||
        header.foldersOffset < 36u) return false;
    if (fseeko(file, static_cast<off_t>(header.foldersOffset), SEEK_SET) != 0) return false;

    std::vector<FolderRecord> folders;
    folders.reserve(header.folderCount);
    for (uint32_t i = 0; i < header.folderCount; ++i) {
        uint8_t record[16]{};
        if (!ReadExact(file, record, sizeof(record))) return false;
        const uint32_t count = ReadLe32(record + 8);
        if (count > header.fileCount) return false;
        folders.push_back(FolderRecord{count});
    }

    std::vector<RawFileRecord> rawFiles;
    rawFiles.reserve(header.fileCount);
    for (const FolderRecord& folder : folders) {
        uint8_t nameLen = 0;
        if (!ReadExact(file, &nameLen, 1) || nameLen == 0) return false;
        std::vector<char> nameBytes(nameLen);
        if (!ReadExact(file, nameBytes.data(), nameBytes.size())) return false;
        if (!nameBytes.empty() && nameBytes.back() == '\0') nameBytes.pop_back();
        const std::string folderName = NormalizePath(
                std::string(nameBytes.begin(), nameBytes.end()));

        for (uint32_t j = 0; j < folder.count; ++j) {
            uint8_t fileRecord[16]{};
            if (!ReadExact(file, fileRecord, sizeof(fileRecord))) return false;
            const uint32_t sizeRaw = ReadLe32(fileRecord + 8);
            RawFileRecord raw;
            raw.folder = folderName;
            raw.size = sizeRaw & 0x3fffffffu;
            raw.offset = ReadLe32(fileRecord + 12);
            raw.compressionToggle = (sizeRaw & 0x40000000u) != 0;
            rawFiles.push_back(std::move(raw));
        }
    }

    const std::string wanted = NormalizePath(requestedPath);
    const std::string wantedShort = WithoutTexturesPrefix(wanted);

    for (RawFileRecord& raw : rawFiles) {
        std::string fileName;
        if (!ReadCString(file, fileName)) return false;
        fileName = NormalizePath(fileName);

        std::string fullPath = raw.folder;
        if (!fullPath.empty() && !fileName.empty()) fullPath += "\\";
        fullPath += fileName;
        fullPath = NormalizePath(fullPath);

        const std::string fullShort = WithoutTexturesPrefix(fullPath);
        if (fullPath == wanted || fullShort == wantedShort) {
            target.found = true;
            target.storedPath = fullPath;
            target.size = raw.size;
            target.offset = raw.offset;
            target.compressionToggle = raw.compressionToggle;
            return true;
        }
    }
    return true;
}

bool InflateZlib(const std::vector<uint8_t>& compressed, uint32_t originalSize,
                 std::vector<uint8_t>& output) {
    if (originalSize == 0 || originalSize > MAX_TARGET_BYTES) return false;
    output.resize(originalSize);
    uLongf outLen = static_cast<uLongf>(output.size());
    const int result = uncompress(reinterpret_cast<Bytef*>(output.data()), &outLen,
                                  reinterpret_cast<const Bytef*>(compressed.data()),
                                  static_cast<uLong>(compressed.size()));
    if (result != Z_OK) {
        output.clear();
        return false;
    }
    output.resize(static_cast<size_t>(outLen));
    return outLen == originalSize;
}

bool ExtractTarget(FILE* file, const BsaHeader& header, const TargetEntry& entry,
                   std::vector<uint8_t>& output, bool& wasCompressed) {
    if (entry.size == 0 || entry.size > MAX_TARGET_BYTES) return false;
    if (fseeko(file, static_cast<off_t>(entry.offset), SEEK_SET) != 0) return false;

    size_t remaining = entry.size;
    const bool embedNames = (header.archiveFlags & 0x100u) != 0;
    if (embedNames) {
        uint8_t nameLen = 0;
        if (!ReadExact(file, &nameLen, 1)) return false;
        if (remaining < static_cast<size_t>(nameLen) + 1u) return false;
        if (fseeko(file, static_cast<off_t>(nameLen), SEEK_CUR) != 0) return false;
        remaining -= static_cast<size_t>(nameLen) + 1u;
    }

    const bool compressedByDefault = (header.archiveFlags & 4u) != 0;
    wasCompressed = compressedByDefault != entry.compressionToggle;

    if (!wasCompressed) {
        if (remaining == 0 || remaining > MAX_TARGET_BYTES) return false;
        output.resize(remaining);
        return ReadExact(file, output.data(), output.size());
    }

    if (remaining < 4u) return false;
    uint8_t sizeBytes[4]{};
    if (!ReadExact(file, sizeBytes, sizeof(sizeBytes))) return false;
    const uint32_t originalSize = ReadLe32(sizeBytes);
    remaining -= 4u;
    if (remaining == 0 || remaining > MAX_TARGET_BYTES) return false;

    std::vector<uint8_t> compressed(remaining);
    if (!ReadExact(file, compressed.data(), compressed.size())) return false;
    return InflateZlib(compressed, originalSize, output);
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

bool TryArchive(const char* archivePath, const std::string& texturePath,
                Fo3RgbaTexture& outTexture) {
    FILE* file = std::fopen(archivePath, "rb");
    if (!file) return false;

    BsaHeader header;
    if (!ReadHeader(file, header)) {
        std::fclose(file);
        return false;
    }
    FQ_LOGI("Q5H TEXTURE BSA OPEN: %s version=%u folders=%u files=%u flags=0x%08X",
            archivePath, header.version, header.folderCount, header.fileCount,
            header.archiveFlags);

    TargetEntry target;
    if (!FindTarget(file, header, texturePath, target)) {
        std::fclose(file);
        FQ_LOGE("Q5H texture BSA index parse failed: %s", archivePath);
        return false;
    }
    if (!target.found) {
        std::fclose(file);
        FQ_LOGW("Q5H texture not found in %s: %s", archivePath, texturePath.c_str());
        return false;
    }

    std::vector<uint8_t> dds;
    bool compressed = false;
    if (!ExtractTarget(file, header, target, dds, compressed)) {
        std::fclose(file);
        FQ_LOGE("Q5H texture extraction failed: %s", target.storedPath.c_str());
        return false;
    }
    std::fclose(file);

    FQ_LOGI("Q5H TEXTURE FOUND: path=%s storedBytes=%u decodedArchiveBytes=%zu compressed=%d",
            target.storedPath.c_str(), target.size, dds.size(), compressed ? 1 : 0);

    if (!DecodeDds(dds, outTexture)) {
        FQ_LOGE("Q5H DDS decode failed: %s bytes=%zu", target.storedPath.c_str(), dds.size());
        return false;
    }

    outTexture.sourcePath = target.storedPath;
    FQ_LOGI("Q5H DDS READY: %dx%d format=%s rgbaBytes=%zu path=%s",
            outTexture.width, outTexture.height, outTexture.format.c_str(),
            outTexture.rgba.size(), outTexture.sourcePath.c_str());
    return true;
}

} // namespace

bool LoadFalloutTextureRgba(const std::string& texturePath, Fo3RgbaTexture& outTexture) {
    outTexture = {};
    if (texturePath.empty()) return false;

    for (const char* archivePath : TEXTURE_BSA_PATHS) {
        if (TryArchive(archivePath, texturePath, outTexture)) return true;
    }

    FQ_LOGE("Q5H FAILED: diffuse texture could not be loaded from either texture BSA name: %s",
            texturePath.c_str());
    return false;
}
