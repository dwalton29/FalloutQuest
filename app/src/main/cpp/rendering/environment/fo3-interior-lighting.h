#pragma once
#include "fo3-esm-reader.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// CPU-only destination snapshot. Never writes live renderer state on a worker.
namespace fo3interior {
struct Cell {
    uint32_t formId=0, imageSpace=0, lightingTemplate=0, inherit=0;
    uint8_t flags=0;
    bool found=false, authored=false;
    std::string editorId;
    std::array<float,3> ambient{}, directional{}, fog{};
    float fogNear=0, fogFar=0, directionalFade=0, fogClip=0, fogPower=0;
    int32_t rotationXY=0, rotationZ=0;
};
struct Reference {
    uint32_t formId=0, base=0, flags=0, parent=0;
    uint8_t parentFlags=0;
    bool transform=false;
    std::array<float,3> position{}, rotation{};
};
struct Light {
    uint32_t ref=0, base=0, flags=0;
    std::string editorId;
    std::array<float,3> position{}, rotation{}, colour{};
    float radius=0, fade=1, falloff=0, fov=0;
};
struct Snapshot {
    Cell cell;
    std::vector<Light> lights;
    std::vector<uint8_t> imagePayload;
    size_t total=0, disabled=0, unsupported=0, unresolvedParents=0;
    bool referencesRead=false;
};
Cell ParseCell(uint32_t formId, const std::vector<uint8_t>& payload);
Reference ParseReference(uint32_t id, uint32_t flags, const std::vector<uint8_t>& payload);
Light ParseLight(uint32_t id, const std::vector<uint8_t>& payload);
std::array<float,3> ToScene(const std::array<float,3>& game, const std::array<float,3>& origin);
bool Load(const std::string& path, uint32_t cell, const std::array<float,3>& origin, Snapshot& out);
constexpr size_t Budget=8; // Quest budget, not a universal PC permutation limit.
struct Selection { int count=0; std::array<size_t,Budget> indices{}; };
Selection Select(const std::vector<Light>& lights, const std::array<float,3>& minimum,
                 const std::array<float,3>& maximum);
float Attenuation(float distance, float radius);
}
