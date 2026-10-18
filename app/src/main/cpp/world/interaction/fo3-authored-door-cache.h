#pragma once

#include "world/interaction/fo3-door-resources.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

// Q16.15: original ESM XTEL/enable-parent semantics are prepared on a
// cancelled, private CPU worker. Only render-thread installation mutates the
// canonical door-resource cache or its publication revision.
struct Fo3AuthoredDoorCpuCache {
    uint32_t cellFormId=0u;
    std::vector<Fo3DoorAnchor> anchors;
    size_t rawReferences=0u, activeReferences=0u, scannedRecords=0u;
    uint64_t preparationUs=0u;
};

bool PrepareFo3AuthoredDoorAnchorsQ2190(uint32_t cellFormId,
    Fo3AuthoredDoorCpuCache& out, const std::atomic<bool>& cancel);
void PublishFo3AuthoredDoorAnchorsQ2190(Fo3AuthoredDoorCpuCache&& prepared);

// Preserve the old synchronous fallback when the worker cannot be started or
// fails, without weakening live ray-hit coverage for original load doors.
void PrimeFo3AuthoredDoorAnchorsQ1870(uint32_t cellFormId);
