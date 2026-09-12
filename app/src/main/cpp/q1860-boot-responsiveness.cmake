# Q16.14: remove the startup ANR without replacing the mature Fallout scene loader.
#
# Device logs show two independent synchronous costs before the first usable frame:
#   - Q10.4 scanned ~307k Wasteland refs, then Q74ResolveOwner reopened/rescanned
#     Fallout3.esm once for every Wasteland XTEL (~240 nested full-file scans).
#   - the mature scene build then spent tens of seconds building/uploading authored
#     meshes and collision while NativeActivity events were serviced only between
#     RenderFrame calls. Android therefore raised an input/focus ANR long before the
#     interaction HUD was used.
#
# Keep all scene/environment semantics byte-for-byte. This pass only:
#   1) resolves the authored Wasteland->Megaton XTEL with ONE ESM pass/index; and
#   2) drains Android NativeActivity events at safe boundaries inside the existing
#      mature CPU/GPU/collision loops so the 5s input watchdog stays serviced.

# -----------------------------------------------------------------------------
# 1. Platform event pump lives in the final OpenXR host TU. It performs the same
#    non-blocking ALooper drain as ProcessAndroidEvents(), but can be called by the
#    synchronous scene loader while RenderFrame is still on the stack.
# -----------------------------------------------------------------------------
set(Q1860_Q4_FILE "${CMAKE_CURRENT_BINARY_DIR}/q1800-q4-generated.cpp")
if(NOT EXISTS "${Q1860_Q4_FILE}")
    message(FATAL_ERROR "Q16.14 expected final q1800 OpenXR source")
endif()
file(READ "${Q1860_Q4_FILE}" Q1860_Q4_SOURCE)

set(Q1860_HOST_ANCHOR [==[
namespace {

constexpr const char* TAG = "FalloutQuest";
]==])
set(Q1860_HOST_NEW [==[
// Q16.14: NativeActivity event bridge for long synchronous Fallout asset work.
// External linkage lets the renderer/collision TUs service Android without
// moving or rewriting any of their authored scene-building behaviour.
static android_app* gFo3AndroidAppQ1860 = nullptr;

void SetFo3AndroidAppQ1860(android_app* app) {
    gFo3AndroidAppQ1860 = app;
}

void PumpFo3AndroidEventsQ1860() {
    android_app* app = gFo3AndroidAppQ1860;
    if (!app) return;

    android_poll_source* source = nullptr;
    int events = 0;
    int processed = 0;
    while (ALooper_pollOnce(0, nullptr, &events,
                            reinterpret_cast<void**>(&source)) >= 0) {
        if (source) {
            source->process(app, source);
            ++processed;
        }
        if (app->destroyRequested) break;
        source = nullptr;
    }

    static uint64_t pumpCalls = 0u;
    static uint64_t processedEvents = 0u;
    ++pumpCalls;
    processedEvents += static_cast<uint64_t>(processed);
    if (processed > 0) {
        __android_log_print(ANDROID_LOG_INFO, "FalloutQuest",
                            "Q16.14 ANDROID EVENT PUMP: call=%llu processed=%d totalEvents=%llu destroy=%d",
                            static_cast<unsigned long long>(pumpCalls), processed,
                            static_cast<unsigned long long>(processedEvents),
                            app->destroyRequested ? 1 : 0);
    }
}

namespace {

constexpr const char* TAG = "FalloutQuest";
]==])
string(FIND "${Q1860_Q4_SOURCE}" "${Q1860_HOST_ANCHOR}" Q1860_HOST_POS)
if(Q1860_HOST_POS EQUAL -1)
    message(FATAL_ERROR "Q16.14 could not find OpenXR namespace anchor")
endif()
string(REPLACE "${Q1860_HOST_ANCHOR}" "${Q1860_HOST_NEW}"
       Q1860_Q4_SOURCE "${Q1860_Q4_SOURCE}")

set(Q1860_CTOR_OLD "    explicit FalloutQuestXr(android_app* app) : app_(app) {}")
set(Q1860_CTOR_NEW [==[
    explicit FalloutQuestXr(android_app* app) : app_(app) {
        SetFo3AndroidAppQ1860(app);
    }
]==])
string(FIND "${Q1860_Q4_SOURCE}" "${Q1860_CTOR_OLD}" Q1860_CTOR_POS)
if(Q1860_CTOR_POS EQUAL -1)
    message(FATAL_ERROR "Q16.14 could not find FalloutQuestXr constructor")
endif()
string(REPLACE "${Q1860_CTOR_OLD}" "${Q1860_CTOR_NEW}"
       Q1860_Q4_SOURCE "${Q1860_Q4_SOURCE}")

# Visible build proof Q16.13 -> Q16.14.
set(Q1860_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.13: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x4Fu); // Q16.13: 3 = A B C D G
]==])
set(Q1860_LABEL_NEW [==[
        q1600Digit(q1600X, 0x06u); // Q16.14: 1 = B C
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x66u); // Q16.14: 4 = F G B C
]==])
string(FIND "${Q1860_Q4_SOURCE}" "${Q1860_LABEL_OLD}" Q1860_LABEL_POS)
if(Q1860_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.14 could not find Q16.13 build-label digits")
endif()
string(REPLACE "${Q1860_LABEL_OLD}" "${Q1860_LABEL_NEW}"
       Q1860_Q4_SOURCE "${Q1860_Q4_SOURCE}")
string(REPLACE "Q16.13 BUILD LABEL:" "Q16.14 BUILD LABEL:"
       Q1860_Q4_SOURCE "${Q1860_Q4_SOURCE}")
string(REPLACE "text=Q16.13 anchor=left-hand" "text=Q16.14 anchor=left-hand"
       Q1860_Q4_SOURCE "${Q1860_Q4_SOURCE}")
string(REPLACE "Q16.13 AUTHORED DOOR FACING" "Q16.14 AUTHORED DOOR FACING"
       Q1860_Q4_SOURCE "${Q1860_Q4_SOURCE}")
file(WRITE "${Q1860_Q4_FILE}" "${Q1860_Q4_SOURCE}")

# -----------------------------------------------------------------------------
# 2. Fast authored gate locator. Q10.4's semantic rule is preserved: discover a
#    Capital Wasteland XTEL whose destination owner belongs to Megaton. The only
#    change is algorithmic: index Megaton REFR owners during the same ESM pass
#    instead of calling Q74ResolveOwner (a fresh full ESM scan) for every XTEL.
# -----------------------------------------------------------------------------
if(NOT EXISTS "${Q720_CELL_SOURCE}")
    message(FATAL_ERROR "Q16.14 expected generated transition source at ${Q720_CELL_SOURCE}")
endif()
file(READ "${Q720_CELL_SOURCE}" Q1860_CELL_SOURCE)
string(PREPEND Q1860_CELL_SOURCE
       "#include <chrono>\n#include <unordered_map>\nextern void PumpFo3AndroidEventsQ1860();\n")

set(Q1860_GATE_IMPL [==[
bool QueueFo3MegatonEntryQ1860() {
    constexpr uint32_t WASTELAND_WORLDSPACE = 0x0000003Cu;
    constexpr uint32_t MEGATON_WORLDSPACE = 0x00000A74u;
    constexpr uint32_t PREFERRED_ENTRANCE_CELL = 0x00002DBDu;

    struct OwnerQ1860 {
        uint32_t cell = 0u;
        uint32_t world = 0u;
    };
    struct CandidateQ1860 {
        uint32_t sourceRef = 0u;
        uint32_t sourceCell = 0u;
        uint32_t destinationRef = 0u;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        float rx = 0.0f, ry = 0.0f, rz = 0.0f;
        uint32_t flags = 0u;
    };

    const auto started = std::chrono::steady_clock::now();
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) {
        Q71_LOGE("Q16.14 FAST GATE SCAN FAILED: reason=open-esm");
        return false;
    }
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        Q71_LOGE("Q16.14 FAST GATE SCAN FAILED: reason=bad-esm-size");
        return false;
    }

    std::vector<GroupFrame> groups;
    std::unordered_map<uint32_t, OwnerQ1860> megatonOwners;
    std::vector<CandidateQ1860> candidates;
    size_t records = 0u;
    size_t wastelandRefs = 0u;
    size_t wastelandXtel = 0u;

    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        ++records;
        if ((records & 0x1fffu) == 0u) PumpFo3AndroidEventsQ1860();

        const uint32_t sizeField = ReadLe32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrame{offset + sizeField,
                                        ReadLe32(header + 8u),
                                        ReadLe32(header + 12u)});
            continue;
        }

        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        uint32_t sourceWorld = 0u;
        uint32_t sourceCell = 0u;
        for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
            if (sourceCell == 0u &&
                (it->type == 6u || it->type == 8u || it->type == 9u || it->type == 10u)) {
                sourceCell = it->label;
            }
            if (sourceWorld == 0u && it->type == 1u) sourceWorld = it->label;
        }

        const uint32_t sourceRef = ReadLe32(header + 12u);
        if (sourceWorld == MEGATON_WORLDSPACE || sourceCell == PREFERRED_ENTRANCE_CELL) {
            megatonOwners[sourceRef] = OwnerQ1860{sourceCell, sourceWorld};
        }

        if (sourceWorld != WASTELAND_WORLDSPACE) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }
        ++wastelandRefs;

        const uint32_t recordFlags = ReadLe32(header + 8u);
        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, recordFlags, payload)) break;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "XTEL", 4u) != 0 || size < 28u) return;
            ++wastelandXtel;
            CandidateQ1860 c;
            c.sourceRef = sourceRef;
            c.sourceCell = sourceCell;
            c.destinationRef = ReadLe32(bytes + 0u);
            c.x = ReadLeFloat(bytes + 4u);
            c.y = ReadLeFloat(bytes + 8u);
            c.z = ReadLeFloat(bytes + 12u);
            c.rx = ReadLeFloat(bytes + 16u);
            c.ry = ReadLeFloat(bytes + 20u);
            c.rz = ReadLeFloat(bytes + 24u);
            c.flags = size >= 32u ? ReadLe32(bytes + 28u) : 0u;
            candidates.push_back(c);
        });
    }
    std::fclose(file);
    PumpFo3AndroidEventsQ1860();

    CandidateQ1860 best{};
    OwnerQ1860 bestOwner{};
    int bestScore = -1;
    size_t resolvedDestinations = 0u;
    size_t megatonTransitions = 0u;
    for (const CandidateQ1860& c : candidates) {
        const auto ownerIt = megatonOwners.find(c.destinationRef);
        if (ownerIt == megatonOwners.end()) continue;
        ++resolvedDestinations;
        const OwnerQ1860& owner = ownerIt->second;
        const bool exactEntranceCell = owner.cell == PREFERRED_ENTRANCE_CELL;
        const bool megatonWorld = owner.world == MEGATON_WORLDSPACE;
        if (!megatonWorld && !exactEntranceCell) continue;
        ++megatonTransitions;

        int score = megatonWorld ? 100 : 50;
        if (exactEntranceCell) score += 100;
        if (score <= bestScore) continue;
        best = c;
        bestOwner = owner;
        bestScore = score;
    }

    const float elapsedMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    Q71_LOGI("Q16.14 FAST GATE SCAN: records=%zu wastelandRefs=%zu wastelandXTEL=%zu ownerIndex=%zu resolvedDest=%zu toMegaton=%zu elapsedMs=%.1f passes=1",
             records, wastelandRefs, wastelandXtel, megatonOwners.size(),
             resolvedDestinations, megatonTransitions, elapsedMs);

    if (bestScore < 0 || best.destinationRef == 0u || bestOwner.cell == 0u) {
        Q71_LOGE("Q16.14 FAST GATE SCAN FAILED: reason=no-wasteland-to-megaton-transition");
        return false;
    }

    gPendingTransitionQ74 = {};
    gPendingTransitionQ74.destinationDoorRef = best.destinationRef;
    gPendingTransitionQ74.cellFormId = bestOwner.cell;
    gPendingTransitionQ74.worldspaceFormId = MEGATON_WORLDSPACE;
    gPendingTransitionQ74.x = best.x;
    gPendingTransitionQ74.y = best.y;
    gPendingTransitionQ74.z = best.z;
    gPendingTransitionQ74.rx = best.rx;
    gPendingTransitionQ74.ry = best.ry;
    gPendingTransitionQ74.rz = best.rz;
    gPendingTransitionQ74.valid = true;
    gHasPendingTransitionQ74 = true;

    Q71_LOGI("Q16.14 FAST GATE READY: sourceDoor=%08X sourceCell=%08X sourceWorld=%08X destinationDoor=%08X destinationCell=%08X destinationWorld=%08X XTEL=(%.2f %.2f %.2f) R=(%.4f %.4f %.4f) score=%d source=Fallout3.esm",
             best.sourceRef, best.sourceCell, WASTELAND_WORLDSPACE,
             best.destinationRef, bestOwner.cell, MEGATON_WORLDSPACE,
             best.x, best.y, best.z, best.rx, best.ry, best.rz, bestScore);
    return true;
}

]==])
set(Q1860_CELL_MARKER [==[
bool ConsumeFo3CellTransitionRequestQ74(Fo3CellTransitionRequestQ74& outRequest) {
]==])
string(FIND "${Q1860_CELL_SOURCE}" "${Q1860_CELL_MARKER}" Q1860_CELL_MARKER_POS)
if(Q1860_CELL_MARKER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.14 could not find transition implementation anchor")
endif()
string(REPLACE "${Q1860_CELL_MARKER}"
       "${Q1860_GATE_IMPL}${Q1860_CELL_MARKER}"
       Q1860_CELL_SOURCE "${Q1860_CELL_SOURCE}")
file(WRITE "${Q720_CELL_SOURCE}" "${Q1860_CELL_SOURCE}")

# -----------------------------------------------------------------------------
# 3. Keep the mature Q16.11 scene-swap function live, but service Android events
#    between its existing authored CPU model builds and GPU uploads.
# -----------------------------------------------------------------------------
set(Q1860_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1860_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.14 expected final q6h native source")
endif()
file(READ "${Q1860_NATIVE_FILE}" Q1860_NATIVE_SOURCE)
string(PREPEND Q1860_NATIVE_SOURCE "extern void PumpFo3AndroidEventsQ1860();\n")

set(Q1860_DECL_OLD "bool QueueFo3MegatonEntryQ1040();")
set(Q1860_DECL_NEW "bool QueueFo3MegatonEntryQ1040();\nbool QueueFo3MegatonEntryQ1860();")
string(FIND "${Q1860_NATIVE_SOURCE}" "${Q1860_DECL_OLD}" Q1860_DECL_POS)
if(Q1860_DECL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.14 could not find Q10.4 gate locator declaration")
endif()
string(REPLACE "${Q1860_DECL_OLD}" "${Q1860_DECL_NEW}"
       Q1860_NATIVE_SOURCE "${Q1860_NATIVE_SOURCE}")
string(FIND "${Q1860_NATIVE_SOURCE}" "if (!QueueFo3MegatonEntryQ1040())" Q1860_GATE_CALL_POS)
if(Q1860_GATE_CALL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.14 could not find live Q10.4 gate locator call")
endif()
string(REPLACE "if (!QueueFo3MegatonEntryQ1040())"
               "if (!QueueFo3MegatonEntryQ1860())"
               Q1860_NATIVE_SOURCE "${Q1860_NATIVE_SOURCE}")

set(Q1860_CPU_LOOP_OLD [==[
    for (const Fo3WorldPlacement& placement : placements) {
        if (Q74ShouldSkipPlacement(placement)) {
]==])
set(Q1860_CPU_LOOP_NEW [==[
    size_t q1860CpuPumpCount = 0u;
    for (const Fo3WorldPlacement& placement : placements) {
        PumpFo3AndroidEventsQ1860();
        ++q1860CpuPumpCount;
        if (Q74ShouldSkipPlacement(placement)) {
]==])
string(FIND "${Q1860_NATIVE_SOURCE}" "${Q1860_CPU_LOOP_OLD}" Q1860_CPU_LOOP_POS)
if(Q1860_CPU_LOOP_POS EQUAL -1)
    message(FATAL_ERROR "Q16.14 could not find mature CPU placement loop")
endif()
string(REPLACE "${Q1860_CPU_LOOP_OLD}" "${Q1860_CPU_LOOP_NEW}"
       Q1860_NATIVE_SOURCE "${Q1860_NATIVE_SOURCE}")

set(Q1860_GPU_LOOP_OLD [==[
    for (CpuObject& cpu : selected) {
        GpuObject gpu;
]==])
set(Q1860_GPU_LOOP_NEW [==[
    size_t q1860GpuPumpCount = 0u;
    for (CpuObject& cpu : selected) {
        PumpFo3AndroidEventsQ1860();
        ++q1860GpuPumpCount;
        GpuObject gpu;
]==])
string(FIND "${Q1860_NATIVE_SOURCE}" "${Q1860_GPU_LOOP_OLD}" Q1860_GPU_LOOP_POS)
if(Q1860_GPU_LOOP_POS EQUAL -1)
    message(FATAL_ERROR "Q16.14 could not find mature GPU upload loop")
endif()
string(REPLACE "${Q1860_GPU_LOOP_OLD}" "${Q1860_GPU_LOOP_NEW}"
       Q1860_NATIVE_SOURCE "${Q1860_NATIVE_SOURCE}")

# Runtime summary immediately before the existing environment/final swap tail.
set(Q1860_COLLISION_CALL_OLD [==[
    const bool collisionReady = InitializeFo3CollisionOverlay(collisionPlacements,
                                                               request.x, request.y, request.z,
                                                               SCENE_FORWARD, FLOOR_Y,
                                                               FO3_UNITS_PER_METRE);
]==])
set(Q1860_COLLISION_CALL_NEW [==[
    Q6H_LOGI("Q16.14 MATURE LOAD PUMP: phase=cpu-gpu-complete cpuBoundaries=%zu gpuBoundaries=%zu collisionPlacements=%zu",
             q1860CpuPumpCount, q1860GpuPumpCount, collisionPlacements.size());
    PumpFo3AndroidEventsQ1860();
    const bool collisionReady = InitializeFo3CollisionOverlay(collisionPlacements,
                                                               request.x, request.y, request.z,
                                                               SCENE_FORWARD, FLOOR_Y,
                                                               FO3_UNITS_PER_METRE);
    PumpFo3AndroidEventsQ1860();
]==])
string(FIND "${Q1860_NATIVE_SOURCE}" "${Q1860_COLLISION_CALL_OLD}" Q1860_COLLISION_CALL_POS)
if(Q1860_COLLISION_CALL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.14 could not find mature collision call")
endif()
string(REPLACE "${Q1860_COLLISION_CALL_OLD}" "${Q1860_COLLISION_CALL_NEW}"
       Q1860_NATIVE_SOURCE "${Q1860_NATIVE_SOURCE}")
file(WRITE "${Q1860_NATIVE_FILE}" "${Q1860_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# 4. Collision build is itself a long authored-NIF loop. Pump between placements
#    in the existing generated collision implementation; do not alter filtering,
#    Havok interpretation, triangle limits, or controller semantics.
# -----------------------------------------------------------------------------
string(PREPEND Q74_COLLISION_SOURCE "extern void PumpFo3AndroidEventsQ1860();\n")
set(Q1860_COLLISION_LOOP_OLD [==[
    bool capped = false;

    for (const Fo3WorldPlacement& placement : placements) {
        if (gPlacementCount >= placementLimitQ78A) {
]==])
set(Q1860_COLLISION_LOOP_NEW [==[
    bool capped = false;
    size_t q1860CollisionPumpCount = 0u;

    for (const Fo3WorldPlacement& placement : placements) {
        PumpFo3AndroidEventsQ1860();
        ++q1860CollisionPumpCount;
        if (gPlacementCount >= placementLimitQ78A) {
]==])
string(FIND "${Q74_COLLISION_SOURCE}" "${Q1860_COLLISION_LOOP_OLD}" Q1860_COLLISION_LOOP_POS)
if(Q1860_COLLISION_LOOP_POS EQUAL -1)
    message(FATAL_ERROR "Q16.14 could not find authored collision placement loop")
endif()
string(REPLACE "${Q1860_COLLISION_LOOP_OLD}" "${Q1860_COLLISION_LOOP_NEW}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# -----------------------------------------------------------------------------
# Configure-time proof: the fast locator is live, the mature environment-aware
# loader remains live, event pumping reaches CPU/GPU/collision, and Q16.13 HUD is
# untouched apart from the visible build number.
# -----------------------------------------------------------------------------
string(FIND "${Q1860_CELL_SOURCE}" "Q16.14 FAST GATE SCAN:" Q1860_FAST_SCAN_OK)
string(FIND "${Q1860_NATIVE_SOURCE}" "QueueFo3MegatonEntryQ1860()" Q1860_FAST_CALL_OK)
string(FIND "${Q1860_NATIVE_SOURCE}" "Q16.11 MATURE SCENE SWAP BEGIN:" Q1860_MATURE_OK)
string(FIND "${Q1860_NATIVE_SOURCE}" "LoadFo3CellEnvironmentQ1410(" Q1860_ENV_OK)
string(FIND "${Q1860_NATIVE_SOURCE}" "Q16.14 MATURE LOAD PUMP:" Q1860_LOAD_PUMP_OK)
string(FIND "${Q74_COLLISION_SOURCE}" "q1860CollisionPumpCount" Q1860_COLLISION_PUMP_OK)
string(FIND "${Q1860_Q4_SOURCE}" "Q16.14 ANDROID EVENT PUMP:" Q1860_HOST_PUMP_OK)
string(FIND "${Q1860_Q4_SOURCE}" "Q16.14: 4 = F G B C" Q1860_LABEL_OK)
string(FIND "${Q1860_Q4_SOURCE}" "Q16.13 HUD TARGET HIDE:" Q1860_HUD_OK)
if(Q1860_FAST_SCAN_OK EQUAL -1 OR Q1860_FAST_CALL_OK EQUAL -1 OR
   Q1860_MATURE_OK EQUAL -1 OR Q1860_ENV_OK EQUAL -1 OR
   Q1860_LOAD_PUMP_OK EQUAL -1 OR Q1860_COLLISION_PUMP_OK EQUAL -1 OR
   Q1860_HOST_PUMP_OK EQUAL -1 OR Q1860_LABEL_OK EQUAL -1 OR
   Q1860_HUD_OK EQUAL -1)
    message(FATAL_ERROR "Q16.14 boot responsiveness verification failed")
endif()

message(STATUS "Q16.14 enabled: one-pass authored gate lookup + Android event servicing inside mature CPU/GPU/collision load")
