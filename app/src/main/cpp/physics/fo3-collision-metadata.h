#pragma once

#include <cstdint>

// Fallout 3 Havok layers from the NIF stream (BS version 34).
// Keep this intentionally conservative: only layers with explicit non-solid
// semantics are rejected by the player collision overlay.
inline bool Fo3HavokLayerBlocksPlayerQ714(uint8_t layer) {
    switch (layer) {
        case 11u: // FOL_WATER
        case 12u: // FOL_TRIGGER
        case 15u: // FOL_NONCOLLIDABLE
        case 16u: // FOL_CLOUD_TRAP
            return false;
        default:
            return true;
    }
}

inline bool Fo3HavokMaterialIsPlatformQ714(uint32_t material) {
    return (material & 0x20u) != 0u;
}

inline bool Fo3HavokMaterialIsStairsQ714(uint32_t material) {
    return (material & 0x40u) != 0u;
}

inline uint32_t Fo3HavokMaterialBaseQ714(uint32_t material) {
    return material & 0x1fu;
}
