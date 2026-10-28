#pragma once

#include "fo3-megaton-scene.h"
#include "fo3-collision-placements.h"
#include "fo3-reference-state.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

struct Fo3CellTransitionRequestQ74 {
    uint32_t destinationDoorRef = 0;
    uint32_t cellFormId = 0;
    uint32_t worldspaceFormId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
    bool valid = false;
};

// CPU-only entry points: no terrain teardown or collision-policy mutation.
// Candidate selection permits empty results and rejects unresolved active bases.
// Current development spawn: original interior Vault 101 cave-door arrival.
bool ResolveFo3StartupEntryQ1860(Fo3CellTransitionRequestQ74& outRequest,
                               const std::atomic<bool>* cancelled = nullptr);
bool ResolveFo3MegatonEntryQ1860(Fo3CellTransitionRequestQ74& outRequest,
                               const std::atomic<bool>* cancelled = nullptr);
bool LoadFo3ScenePlacements(const Fo3CellTransitionRequestQ74& request,
                           std::vector<Fo3WorldPlacement>& outPlacements,
                           const fo3world::ReferenceStateSnapshot* referenceState = nullptr,
                           bool candidateSelection = false);
// Render thread publishes a resolved startup context before finalization.
void SetFo3TransitionContextQ74(const Fo3CellTransitionRequestQ74& request);

// Renderer-side transition queue. The input callback only enqueues; GL/collision
// teardown and rebuild happen later from the render thread.
bool ConsumeFo3CellTransitionRequestQ74(Fo3CellTransitionRequestQ74& outRequest);
bool HasPendingFo3CellTransitionRequestQ74() noexcept;
void CompleteFo3CellTransitionQ74(uint32_t cellFormId);

// Consumed by the authored collision resolver on the frame after a successful
// scene swap. Returning the body centre to VR (0,0) maps the player to XTEL.
bool ConsumeFo3PlayerResetQ74();

// Generic ESM4 CELL loader used by Q7.4 after the door has resolved its owning
// destination CELL. Only active, model-bearing REFRs are returned.
bool LoadFo3CellPlacementsQ74(uint32_t cellFormId,
                             std::vector<Fo3WorldPlacement>& outPlacements,
                           const fo3world::ReferenceStateSnapshot* referenceState = nullptr);

// Q7.5 exterior loader. The XTEL-linked CELL can be a worldspace persistent
// cell, so this discovers the WRLD's XCLC grid cells, merges the persistent
// refs with the relevant exterior cells around the arrival point (or all cells
// for a small dedicated worldspace), and returns one combined placement set.
bool LoadFo3WorldspaceNeighborhoodQ75(uint32_t worldspaceFormId,
                                     uint32_t persistentCellFormId,
                                     float arrivalX, float arrivalY,
                                     std::vector<Fo3WorldPlacement>& outPlacements,
                           const fo3world::ReferenceStateSnapshot* referenceState = nullptr);

// Q7.10 collision classification. Q7.8a proved that blindly welding every bhk
// model into the static world makes Fallout's movable clutter behave like
// concrete. Keep a small process-wide set of model paths used exclusively by
// dynamic/item record types in the currently loaded CELL placement set.
inline std::string NormalizeFo3CollisionModelPathQ710(std::string path) {
    for (char& ch : path) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        if (ch == '/') ch = '\\';
    }
    return path;
}

inline bool IsFo3DynamicCollisionRecordTypeQ710(const std::string& type) {
    return type == "MISC" || type == "MSTT" || type == "ALCH" ||
           type == "AMMO" || type == "WEAP" || type == "ARMO" ||
           type == "BOOK" || type == "KEYM" || type == "INGR";
}

inline std::unordered_set<std::string>& Fo3DynamicOnlyCollisionModelsQ710() {
    static std::unordered_set<std::string> models;
    return models;
}

inline void ClearFo3CollisionPolicyQ710() {
    Fo3DynamicOnlyCollisionModelsQ710().clear();
}

inline std::unordered_set<std::string> BuildFo3CollisionPolicyQ710(
        const std::vector<Fo3WorldPlacement>& placements) {
    std::unordered_set<std::string> dynamicModels;
    std::unordered_set<std::string> staticModels;
    for (const Fo3WorldPlacement& placement : placements) {
        if (placement.modelPath.empty()) continue;
        const std::string model = NormalizeFo3CollisionModelPathQ710(placement.modelPath);
        if (IsFo3DynamicCollisionRecordTypeQ710(placement.baseRecordType)) {
            dynamicModels.insert(model);
        } else {
            staticModels.insert(model);
        }
    }

    std::unordered_set<std::string> dynamicOnly;
    for (const std::string& model : dynamicModels) {
        if (staticModels.find(model) == staticModels.end()) dynamicOnly.insert(model);
    }
    return dynamicOnly;
}

inline size_t ConfigureFo3CollisionPolicyQ710(const std::vector<Fo3WorldPlacement>& placements) {
    auto next=BuildFo3CollisionPolicyQ710(placements);
    Fo3DynamicOnlyCollisionModelsQ710().swap(next);
    return Fo3DynamicOnlyCollisionModelsQ710().size();
}

inline bool ShouldLoadFo3StaticCollisionModelQ710(const std::string& modelPath) {
    if (modelPath.empty()) return false;
    const std::string model = NormalizeFo3CollisionModelPathQ710(modelPath);
    return Fo3DynamicOnlyCollisionModelsQ710().find(model) ==
           Fo3DynamicOnlyCollisionModelsQ710().end();
}

// The generated collision translation unit includes the Q6F declaration before
// this header. Other translation units only need this harmless forward
// declaration. The wrapper is intentionally model-level: Q7.10 configures the
// dynamic-only model set from the authoritative REFR/base record types before
// InitializeFo3CollisionOverlay runs.
struct Fo3NifCollisionShapeQ6F;
bool LoadFo3NifCollisionShapesQ6F(
        const std::string& modelPath,
        std::vector<Fo3NifCollisionShapeQ6F>& outShapes);

inline bool LoadFo3NifCollisionShapesPolicyQ710(
        const std::string& modelPath,
        std::vector<Fo3NifCollisionShapeQ6F>& outShapes) {
    // Do not touch outShapes here: this header is also included by translation
    // units that only have a forward declaration of Fo3NifCollisionShapeQ6F.
    // The collision loader creates a fresh empty vector for every cache miss,
    // so returning false is sufficient for a filtered dynamic-only model.
    if (!ShouldLoadFo3StaticCollisionModelQ710(modelPath)) return false;
    return LoadFo3NifCollisionShapesQ6F(modelPath, outShapes);
}

// Calls in the generated collision source after this include are routed through
// the Q7.10 policy. The original Q6F implementation lives in another source
// file and is compiled without this header, so the wrapper can safely call it.
#define LoadFo3NifCollisionShapesQ6F LoadFo3NifCollisionShapesPolicyQ710
