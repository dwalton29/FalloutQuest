#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Loads one path from Fallout - Meshes.bsa. The BSA index is cached after the
// first call so Q6 can resolve many ESM-driven model paths without rescanning
// all ~14k archive entries for every object.
bool LoadFalloutMeshFile(const std::string& modelPath,
                         std::vector<uint8_t>& outBytes,
                         std::string* outResolvedPath = nullptr);
