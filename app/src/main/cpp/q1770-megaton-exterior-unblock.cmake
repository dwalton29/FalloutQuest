# Q16.7: make the first Capital Wasteland transition practical on Quest.
#
# Device testing reached the exterior but then stalled, and the closed Megaton
# gate/fuselage geometry trapped the player in the authored entrance pocket.
# Inspection of Fallout3.esm shows the Q7.5 5x5 neighbourhood around the gate
# contains ~3.3k local REFRs, while the persistent CELL contributes ~4k more
# globally-persistent REFRs that are nowhere near Megaton. Q16.7 therefore:
#   - temporarily reduces the exterior neighbourhood radius from 2 to 1 (3x3),
#   - spatially culls persistent-CELL refs to that same local neighbourhood,
#   - suppresses the three authored Megaton entrance pieces on the exterior side
#     so the exit is open air until scripted gate animation is implemented.
#
# Interior traversal, XTEL ownership, Q16.6 loading UI and Q16.5 arm HUD remain
# unchanged. Start from Q7.20's generated worldspace source so its authored
# XCLC Force-Hide-Land handling is preserved.

set(Q1770_WORLD_INPUT "${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q720.cpp")
if(NOT EXISTS "${Q1770_WORLD_INPUT}")
    message(FATAL_ERROR "Q16.7 expected Q7.20 generated worldspace at ${Q1770_WORLD_INPUT}")
endif()
file(READ "${Q1770_WORLD_INPUT}" Q1770_WORLD_SOURCE)

# 1) Keep the first open-world test light enough for a synchronous Quest swap.
string(FIND "${Q1770_WORLD_SOURCE}" "constexpr int GRID_RADIUS_Q75 = 2;" Q1770_RADIUS_OLD_POS)
if(Q1770_RADIUS_OLD_POS EQUAL -1)
    message(FATAL_ERROR "Q16.7 could not find Q7.20 exterior grid radius")
endif()
string(REPLACE
    "constexpr int GRID_RADIUS_Q75 = 2;"
    "constexpr int GRID_RADIUS_Q75 = 1; // Q16.7 temporary 3x3 exterior test window"
    Q1770_WORLD_SOURCE "${Q1770_WORLD_SOURCE}")

# 2) Remember which CELL each raw placement came from so the persistent CELL can
# be spatially clipped without touching ordinary grid-cell placements.
string(FIND "${Q1770_WORLD_SOURCE}"
    "struct RawPlacementQ75 {\n    uint32_t refFormId = 0;"
    Q1770_RAW_STRUCT_POS)
if(Q1770_RAW_STRUCT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.7 could not find RawPlacementQ75")
endif()
string(REPLACE
    "struct RawPlacementQ75 {\n    uint32_t refFormId = 0;"
    "struct RawPlacementQ75 {\n    uint32_t refFormId = 0;\n    uint32_t owningCellFormId = 0; // Q16.7 persistent-cell locality"
    Q1770_WORLD_SOURCE "${Q1770_WORLD_SOURCE}")

string(FIND "${Q1770_WORLD_SOURCE}"
    "placement.refFormId = formId;\n        placement.recordFlags = flags;"
    Q1770_OWNER_ASSIGN_POS)
if(Q1770_OWNER_ASSIGN_POS EQUAL -1)
    message(FATAL_ERROR "Q16.7 could not find raw placement owner assignment point")
endif()
string(REPLACE
    "placement.refFormId = formId;\n        placement.recordFlags = flags;"
    "placement.refFormId = formId;\n        placement.owningCellFormId = cell;\n        placement.recordFlags = flags;"
    Q1770_WORLD_SOURCE "${Q1770_WORLD_SOURCE}")

# Cull persistent refs to the same grid window as the selected exterior cells.
set(Q1770_COLLECT_OLD [==[
    if (!CollectSelectedRefsQ75(worldspaceFormId, selectedCells, raw, landRecords)) {
        Q75_LOGE("Q7.5 WORLDSPACE LOAD FAILED: worldspace=%08X reason=collect-refs selectedCells=%zu",
                 worldspaceFormId, selectedCells.size());
        return false;
    }

    std::unordered_map<uint32_t, const RawPlacementQ75*> refs;
]==])
set(Q1770_COLLECT_NEW [==[
    if (!CollectSelectedRefsQ75(worldspaceFormId, selectedCells, raw, landRecords)) {
        Q75_LOGE("Q7.5 WORLDSPACE LOAD FAILED: worldspace=%08X reason=collect-refs selectedCells=%zu",
                 worldspaceFormId, selectedCells.size());
        return false;
    }

    size_t q1770PersistentBefore = 0u;
    for (const RawPlacementQ75& p : raw) {
        if (p.owningCellFormId == persistentCellFormId) ++q1770PersistentBefore;
    }
    raw.erase(std::remove_if(raw.begin(), raw.end(),
                             [&](const RawPlacementQ75& p) {
        if (p.owningCellFormId != persistentCellFormId) return false;
        const int32_t gx = static_cast<int32_t>(std::floor(p.x / EXTERIOR_CELL_SIZE_Q75));
        const int32_t gy = static_cast<int32_t>(std::floor(p.y / EXTERIOR_CELL_SIZE_Q75));
        return std::abs(gx - targetGridX) > GRID_RADIUS_Q75 ||
               std::abs(gy - targetGridY) > GRID_RADIUS_Q75;
    }), raw.end());
    size_t q1770PersistentAfter = 0u;
    for (const RawPlacementQ75& p : raw) {
        if (p.owningCellFormId == persistentCellFormId) ++q1770PersistentAfter;
    }
    Q75_LOGI("Q16.7 PERSISTENT LOCAL CULL: worldspace=%08X persistent=%08X before=%zu kept=%zu removed=%zu targetGrid=(%d,%d) radius=%d",
             worldspaceFormId, persistentCellFormId,
             q1770PersistentBefore, q1770PersistentAfter,
             q1770PersistentBefore - q1770PersistentAfter,
             targetGridX, targetGridY, GRID_RADIUS_Q75);

    std::unordered_map<uint32_t, const RawPlacementQ75*> refs;
]==])
string(FIND "${Q1770_WORLD_SOURCE}" "${Q1770_COLLECT_OLD}" Q1770_COLLECT_POS)
if(Q1770_COLLECT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.7 could not find Q7.20 raw placement collection block")
endif()
string(REPLACE "${Q1770_COLLECT_OLD}" "${Q1770_COLLECT_NEW}"
       Q1770_WORLD_SOURCE "${Q1770_WORLD_SOURCE}")

# 3) Temporarily remove the exterior Megaton entrance assembly. These are the
# exact Fallout3.esm refs at the arrival point:
#   00003B24 MegatonExteriorGateRef / MegatonMainGate01.NIF
#   0001D55A MegatonGateHouseREF    / MegatonGateHouse01.NIF
#   0006D4AD MegatonGateHouse01Dest / MegatonGateDest01.NIF
# Skipping them from outPlacements removes both their render and collision paths.
set(Q1770_ASSEMBLY_OLD [==[
    std::unordered_map<std::string, size_t> baseTypes;
    std::unordered_set<std::string> uniqueModels;
    for (const RawPlacementQ75* p : active) {
        const auto it = bases.find(p->baseFormId);
        if (it == bases.end()) continue;
        const BaseRecordQ75& base = it->second;
        ++baseTypes[base.recordType.empty() ? "<none>" : base.recordType];
]==])
set(Q1770_ASSEMBLY_NEW [==[
    std::unordered_map<std::string, size_t> baseTypes;
    std::unordered_set<std::string> uniqueModels;
    size_t q1770MegatonEntranceSkipped = 0u;
    for (const RawPlacementQ75* p : active) {
        const auto it = bases.find(p->baseFormId);
        if (it == bases.end()) continue;
        const BaseRecordQ75& base = it->second;

        const bool q1770SuppressMegatonEntrance =
            worldspaceFormId == 0x0000003Cu &&
            (p->refFormId == 0x00003B24u ||
             p->refFormId == 0x0001D55Au ||
             p->refFormId == 0x0006D4ADu);
        if (q1770SuppressMegatonEntrance) {
            ++q1770MegatonEntranceSkipped;
            Q75_LOGI("Q16.7 MEGATON ENTRANCE SUPPRESSED: ref=%08X base=%08X EDID=%s model=%s",
                     p->refFormId, p->baseFormId,
                     base.editorId.empty() ? "<none>" : base.editorId.c_str(),
                     base.modelPath.empty() ? "<none>" : base.modelPath.c_str());
            continue;
        }

        ++baseTypes[base.recordType.empty() ? "<none>" : base.recordType];
]==])
string(FIND "${Q1770_WORLD_SOURCE}" "${Q1770_ASSEMBLY_OLD}" Q1770_ASSEMBLY_POS)
if(Q1770_ASSEMBLY_POS EQUAL -1)
    message(FATAL_ERROR "Q16.7 could not find Q7.20 placement assembly loop")
endif()
string(REPLACE "${Q1770_ASSEMBLY_OLD}" "${Q1770_ASSEMBLY_NEW}"
       Q1770_WORLD_SOURCE "${Q1770_WORLD_SOURCE}")

string(REPLACE
    "    Q75_LOGI(\"Q7.5 WORLDSPACE READY: worldspace=%08X persistent=%08X selectedCells=%zu LAND=%zu rawRefs=%zu activeRefs=%zu disabledInitial=%zu bases=%zu modelPlacements=%zu uniqueModels=%zu XTEL=(%.2f %.2f)\","
    "    Q75_LOGI(\"Q16.7 EXTERIOR TEST READY: entranceSkipped=%zu persistentKept=%zu radius=%d\", q1770MegatonEntranceSkipped, q1770PersistentAfter, GRID_RADIUS_Q75);\n    Q75_LOGI(\"Q7.5 WORLDSPACE READY: worldspace=%08X persistent=%08X selectedCells=%zu LAND=%zu rawRefs=%zu activeRefs=%zu disabledInitial=%zu bases=%zu modelPlacements=%zu uniqueModels=%zu XTEL=(%.2f %.2f)\","
    Q1770_WORLD_SOURCE "${Q1770_WORLD_SOURCE}")

set(Q1770_WORLD_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q1770.cpp")
file(WRITE "${Q1770_WORLD_OUTPUT}" "${Q1770_WORLD_SOURCE}")

# Route the already-generated Q16 transition translation unit from Q7.20's
# generated worldspace source to Q16.7's generated derivative. Q16.0 has already
# added its traversal code to this translation unit, so patch only the include.
if(NOT EXISTS "${Q720_CELL_SOURCE}")
    message(FATAL_ERROR "Q16.7 expected generated transition source at ${Q720_CELL_SOURCE}")
endif()
file(READ "${Q720_CELL_SOURCE}" Q1770_CELL_SOURCE)
set(Q1770_WORLD_INCLUDE_OLD
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q720.cpp\"")
set(Q1770_WORLD_INCLUDE_NEW
    "#include \"${Q1770_WORLD_OUTPUT}\"")
string(FIND "${Q1770_CELL_SOURCE}" "${Q1770_WORLD_INCLUDE_OLD}" Q1770_INCLUDE_POS)
if(Q1770_INCLUDE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.7 could not find Q7.20 generated worldspace include in transition source")
endif()
string(REPLACE
    "${Q1770_WORLD_INCLUDE_OLD}"
    "${Q1770_WORLD_INCLUDE_NEW}"
    Q1770_CELL_SOURCE "${Q1770_CELL_SOURCE}")
file(WRITE "${Q720_CELL_SOURCE}" "${Q1770_CELL_SOURCE}")

# Visible headset proof: Q16.6 -> Q16.7.
set(Q1770_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1770_Q4_INPUT}")
    message(FATAL_ERROR "Q16.7 expected final OpenXR source at ${Q1770_Q4_INPUT}")
endif()
file(READ "${Q1770_Q4_INPUT}" Q1770_Q4_SOURCE)
set(Q1770_LABEL_OLD [==[
        q1600Digit(q1600X, 0x7Du); // Q16.6: 6 = A F G E D C
]==])
set(Q1770_LABEL_NEW [==[
        q1600Digit(q1600X, 0x07u); // Q16.7: 7 = A B C
]==])
string(FIND "${Q1770_Q4_SOURCE}" "${Q1770_LABEL_OLD}" Q1770_LABEL_POS)
if(Q1770_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.7 could not find Q16.6 final build-label digit")
endif()
string(REPLACE "${Q1770_LABEL_OLD}" "${Q1770_LABEL_NEW}"
       Q1770_Q4_SOURCE "${Q1770_Q4_SOURCE}")
string(REPLACE "Q16.6 BUILD LABEL:" "Q16.7 BUILD LABEL:"
       Q1770_Q4_SOURCE "${Q1770_Q4_SOURCE}")
string(REPLACE "text=Q16.6 anchor=left-hand" "text=Q16.7 anchor=left-hand"
       Q1770_Q4_SOURCE "${Q1770_Q4_SOURCE}")
file(WRITE "${Q1770_Q4_INPUT}" "${Q1770_Q4_SOURCE}")

# Configure-time proof that this is the lightweight exterior/unblocked build and
# that Q7.20 terrain semantics + latest UI work remain present.
string(FIND "${Q1770_WORLD_SOURCE}" "GRID_RADIUS_Q75 = 1" Q1770_RADIUS_OK)
string(FIND "${Q1770_WORLD_SOURCE}" "forceHideLandQ720" Q1770_FORCE_HIDE_OK)
string(FIND "${Q1770_WORLD_SOURCE}" "Q16.7 PERSISTENT LOCAL CULL" Q1770_CULL_OK)
string(FIND "${Q1770_WORLD_SOURCE}" "0x00003B24u" Q1770_GATE_OK)
string(FIND "${Q1770_WORLD_SOURCE}" "0x0001D55Au" Q1770_HOUSE_OK)
string(FIND "${Q1770_WORLD_SOURCE}" "0x0006D4ADu" Q1770_DEST_OK)
string(FIND "${Q1770_CELL_SOURCE}" "fo3-worldspace-q1770.cpp" Q1770_ROUTE_OK)
string(FIND "${Q1770_Q4_SOURCE}" "Q16.7: 7 = A B C" Q1770_LABEL_OK)
string(FIND "${Q1770_Q4_SOURCE}" "RenderFo3LoadingScreenQ1760" Q1770_LOADING_OK)
string(FIND "${Q1770_Q4_SOURCE}" "q1750ButtonSegments = 32" Q1770_HUD_OK)
if(Q1770_RADIUS_OK EQUAL -1 OR Q1770_FORCE_HIDE_OK EQUAL -1 OR
   Q1770_CULL_OK EQUAL -1 OR Q1770_GATE_OK EQUAL -1 OR
   Q1770_HOUSE_OK EQUAL -1 OR Q1770_DEST_OK EQUAL -1 OR
   Q1770_ROUTE_OK EQUAL -1 OR Q1770_LABEL_OK EQUAL -1 OR
   Q1770_LOADING_OK EQUAL -1 OR Q1770_HUD_OK EQUAL -1)
    message(FATAL_ERROR "Q16.7 verification failed")
endif()

message(STATUS "Q16.7 exterior test enabled: Q7.20 terrain + 3x3 local window + persistent-cell cull + Megaton entrance removed")
