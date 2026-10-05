#pragma once
#include "fo3-map-runtime.h"
namespace fo3pipdata {
uint64_t LocalMapGeometryRevision();
// Render-thread snapshot of already transformed authored collision; no ESM/NIF
// I/O.
std::shared_ptr<const LocalMap>
CaptureLocalMap(uint32_t cell, uint64_t generation, float headY);
} // namespace fo3pipdata
