#pragma once

#include "fo3-collision-metadata-q714.h"
#include "fo3-nif-collision-q6f.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct Fo3PackedSubShapeMetadataQ714 {
    uint8_t layer = 0u;
    uint8_t flagsAndPartNumber = 0u;
    uint16_t group = 0u;
    uint32_t material = 0u;
    uint32_t firstVertex = 0u;
    uint32_t vertexCount = 0u;

    bool BlocksPlayer() const { return Fo3HavokLayerBlocksPlayerQ714(layer); }
    bool IsStairs() const { return Fo3HavokMaterialIsStairsQ714(material); }
    bool IsPlatform() const { return Fo3HavokMaterialIsPlatformQ714(material); }
};

using Fo3PackedMetadataMapQ714 =
    std::unordered_map<uint32_t, std::vector<Fo3PackedSubShapeMetadataQ714>>;

inline bool ParseFo3PackedSubShapeMetadataQ714(
        const uint8_t* data, size_t size,
        std::vector<Fo3PackedSubShapeMetadataQ714>& out) {
    using namespace fo3_q6f_collision_detail;
    out.clear();
    if (!data || size < 11u) return false;

    Cursor c(data, size);
    uint32_t triangleCount = 0u;
    if (!c.U32(triangleCount) || triangleCount == 0u ||
        triangleCount > MAX_COLLISION_TRIANGLES) return false;
    if (!c.Skip(static_cast<size_t>(triangleCount) * 8u)) return false;

    uint32_t vertexCount = 0u;
    uint8_t compressed = 0u;
    if (!c.U32(vertexCount) || vertexCount == 0u ||
        vertexCount > MAX_COLLISION_VERTICES || !c.U8(compressed)) return false;

    const size_t bytesPerVertex = compressed != 0u ? 6u : 12u;
    if (!c.Skip(static_cast<size_t>(vertexCount) * bytesPerVertex)) return false;

    uint16_t subShapeCount = 0u;
    if (!c.U16(subShapeCount)) return false;
    out.reserve(subShapeCount);

    uint64_t firstVertex = 0u;
    for (uint16_t i = 0u; i < subShapeCount; ++i) {
        Fo3PackedSubShapeMetadataQ714 meta;
        // FO3 20.2.0.7 hkSubPartData is HavokFilter, Num Vertices, Material.
        // Q7.14 accidentally read Material before Num Vertices, which made most
        // packed triangles fail subshape assignment and lose Bethesda metadata.
        if (!c.U8(meta.layer) || !c.U8(meta.flagsAndPartNumber) ||
            !c.U16(meta.group) || !c.U32(meta.vertexCount) ||
            !c.U32(meta.material)) {
            out.clear();
            return false;
        }
        if (firstVertex > vertexCount ||
            meta.vertexCount > vertexCount - static_cast<uint32_t>(firstVertex)) {
            out.clear();
            return false;
        }
        meta.firstVertex = static_cast<uint32_t>(firstVertex);
        firstVertex += meta.vertexCount;
        out.push_back(meta);
    }

    // FO3 20.2.0.7 packed data ends after the hkSubPartData array.
    // Allow an incomplete partition defensively, but never accept trailing bytes;
    // that catches a bad layout assumption before it can alter collision policy.
    return c.remaining() == 0u;
}

inline bool LoadFo3PackedMetadataQ714(const std::string& modelPath,
                                      Fo3PackedMetadataMapQ714& out) {
    using namespace fo3_q6f_collision_detail;
    out.clear();

    std::vector<uint8_t> nif;
    std::string resolved;
    if (!LoadFalloutMeshFile(modelPath, nif, &resolved) || nif.empty() ||
        nif.size() > MAX_NIF_BYTES) return false;

    NifHeader header;
    if (!ParseHeader(nif, header)) return false;

    for (uint32_t block = 0u; block < header.numBlocks; ++block) {
        if (BlockType(header, block) != "hkPackedNiTriStripsData") continue;
        std::vector<Fo3PackedSubShapeMetadataQ714> metadata;
        if (ParseFo3PackedSubShapeMetadataQ714(
                BlockData(nif, header, block), header.blockSizes[block], metadata) &&
            !metadata.empty()) {
            out.emplace(block, std::move(metadata));
        }
    }
    return !out.empty();
}

inline uint16_t FindFo3PackedSubShapeForTriangleQ714(
        const std::vector<Fo3PackedSubShapeMetadataQ714>& metadata,
        uint32_t ia, uint32_t ib, uint32_t ic) {
    for (uint16_t i = 0u; i < metadata.size() && i != 0xffffu; ++i) {
        const Fo3PackedSubShapeMetadataQ714& meta = metadata[i];
        const uint64_t end = static_cast<uint64_t>(meta.firstVertex) + meta.vertexCount;
        const auto inside = [&](uint32_t v) {
            return v >= meta.firstVertex && static_cast<uint64_t>(v) < end;
        };
        if (inside(ia) && inside(ib) && inside(ic)) return i;
    }
    return 0xffffu;
}
