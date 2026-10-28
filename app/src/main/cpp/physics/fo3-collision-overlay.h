#pragma once

#include "fo3-megaton-scene.h"

#include <vector>
#include <atomic>
#include <array>
#include <unordered_map>
#include <unordered_set>

// Render-thread world removal; reapplied when a rolling snapshot publishes.
void SetFo3CollectedCollisionRefs(const std::unordered_set<uint32_t>& refs);
bool SetFo3DoorCollisionTransformQ2400(uint32_t refFormId,const float transform[16],bool enabled);
bool HasFo3InteractionOccluder(float ox,float oy,float oz,float dx,float dy,float dz,
                              float distance,uint32_t targetRef);

// Builds the shared authored-collision world from Bethesda NIF bhk shapes for
// active CELL static placements. Q6F can render it as a debug overlay;
// Q6G consumes the exact same transformed triangles for locomotion.
bool InitializeFo3CollisionOverlay(const std::vector<Fo3WorldPlacement>& placements,
                                   float centerX, float centerY, float floorZ,
                                   float sceneForward, float floorY,
                                   float unitsPerMetre);

// Explicit CELL replacement clears transformed/negative REFR chunks without
// retiring the live world. Call after background work drains, before preparation.
void ResetFo3CollisionSceneCache();

// Q18.2: prepare one exterior REFR's transformed authored bhk chunk without
// replacing the currently active collision world. The streaming runtime calls
// this under a small per-frame budget, then performs a cheap cached 3x3 publish.
bool IsFo3CollisionPlacementCachedQ1820(uint32_t refFormId);
bool PrimeFo3CollisionPlacementCacheQ1820(
    const Fo3WorldPlacement& placement,
    float centerX, float centerY, float floorZ,
    float sceneForward, float floorY, float unitsPerMetre,
    size_t* outTriangles);

// Q19.3: build the complete exterior 3x3 collision snapshot off the gameplay
// thread from already-prewarmed REFR chunks. The returned token owns triangles,
// coherent shapes, authored weld adjacency, and Q9.5 broadphase until published.
bool PrepareFo3CollisionSnapshotQ1930(
    const std::vector<Fo3WorldPlacement>& placements,
    float centerX, float centerY, float floorZ,
    float sceneForward, float floorY, float unitsPerMetre,
    uint64_t* outToken, bool exterior = true, uint32_t cellFormId = 0u);

bool PublishFo3CollisionSnapshotQ1930(uint64_t token, uint64_t* outSwapUs);
void DiscardFo3CollisionSnapshotQ1930(uint64_t token);

// M1B3: render-thread collision publication generation. Capture this together
// with the scene stamp before scheduling serialized asset-worker preparation.
uint64_t GetFo3CollisionPublicationRevision();
struct Fo3DoorCollisionPose {
    std::array<float,16> transform{{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}};
    std::unordered_map<uint32_t,std::array<float,16>> nodeTransforms;
    bool enabled = true;
    bool operator==(const Fo3DoorCollisionPose& other) const {
        return enabled == other.enabled && transform == other.transform && nodeTransforms == other.nodeTransforms;
    }
};
struct Fo3CollisionMutationState {
    uint64_t revision = 0u;
    std::unordered_set<uint32_t> collected;
    std::unordered_map<uint32_t,Fo3DoorCollisionPose> doors;
    std::unordered_set<std::string> dynamicOnlyModels;
};
// Render-thread capture. The returned value owns all mutable inputs needed by
// the asset worker, including open/moving/rotated interior-door collision.
Fo3CollisionMutationState CaptureFo3CollisionMutationState();
// Prepare a reversible candidate without touching the live collision world.
// Placements must already reflect the candidate enable-parent projection and
// have prewarmed authored bhk data in the current transform context. Collected
// references are a copied transaction input, never read from live Player state.
// Empty selections are valid prepared worlds; capped/unprimed worlds reject.
bool PrepareFo3CollisionMutationSnapshot(
    const std::vector<Fo3WorldPlacement>& placements,
    float centerX, float centerY, float floorZ,
    float sceneForward, float floorY, float unitsPerMetre,
    const Fo3CollisionMutationState& state,
    uint64_t* outToken, bool exterior = true, uint32_t cellFormId = 0u);
// Private resident candidate: decodes complete supported collision chains into
// local chunks; never reads/writes transformed/negative caches or changes the
// live model policy. Candidate policy is computed from this collision scope.
// Must run on the exclusive asset lane. Caller owns the returned registry token.
bool PrepareFo3ResidentCollisionSnapshot(
    const std::vector<Fo3WorldPlacement>& placements,
    float centerX,float centerY,float floorZ,float sceneForward,float floorY,float unitsPerMetre,
    const Fo3CollisionMutationState& published,const std::atomic<bool>& cancelled,
    uint64_t* outToken,bool exterior=true,uint32_t cellFormId=0u);
// Same render-thread turn as the other scene exchanges. On success the token
// owns the displaced world, so call again to roll back, or Discard to finalize.
// No allocations/derived rebuilds occur in the exchange. Any intervening live
// collision mutation invalidates the token's expected revision; stale tokens
// fail without touching either world and may still be discarded safely.
bool CanExchangeFo3CollisionMutationSnapshot(uint64_t token);
bool ExchangeFo3CollisionMutationSnapshot(uint64_t token);

// Resolves a standing player capsule against the Q6F-authored Fallout collision
// world. X/Z are the virtual headset/body centre in metres. playerYOffset is the
// locomotion-space Y translation; grounding updates it relative to floorY.
// Returns false only when the collision world is unavailable.
bool ResolveFo3PlayerMotionQ6G(float currentX, float currentZ,
                               float desiredX, float desiredZ,
                               float currentPlayerYOffset,
                               float* outX, float* outZ,
                               float* outPlayerYOffset);

// Ready means a world was published, including an intentionally empty world.
// It does not imply a ground surface exists. Shutdown/uninitialized stays false.
bool IsFo3PlayerCollisionReadyQ6G();
// Contact at the resolved capsule feet; false for unsupported/airborne positions.
bool SampleFo3PlayerFootstepSurface(float x,float z,float playerYOffset,uint32_t* material);

// Resident actors use NAVM for route topology but the visible root height must
// follow the same authored Havok/LAND surfaces as player locomotion.
bool SampleFo3ActorGroundQ240(float x, float z, float referenceY,
                              float* outGroundY);

// Q22.3: resolve a moving loose object's authored-collision bounding sphere
// against the currently active authored Fallout collision world. The moving
// REFR is excluded from contact tests so its original static bhk placement
// cannot collide with itself after pickup.
bool ResolveFo3DynamicBoxQ225(
    uint32_t movingRefFormId,
    float currentX, float currentY, float currentZ,
    float desiredX, float desiredY, float desiredZ,
    float halfX, float halfY, float halfZ,
    float axisXx, float axisXy, float axisXz,
    float axisYx, float axisYy, float axisYz,
    float axisZx, float axisZy, float axisZz,
    float broadRadius,
    float* outX, float* outY, float* outZ,
    float* outNormalX, float* outNormalY, float* outNormalZ,
    uint32_t* outContacts,
    uint32_t* outCandidates);

// Q6F debug draw. Q6G keeps this disabled by default while retaining the same
// collision data for physical movement. The symbol is weak so Q7.6b can replace
// only this disabled visual hook with the exterior LAND renderer; none of the
// collision/grounding functions above are replaced or modified.
void RenderFo3CollisionOverlay(const float* mvp16) __attribute__((weak));

void ShutdownFo3CollisionOverlay();

// Authored controller deltas keyed by collision target AVObject block.
bool SetFo3ScriptCollisionPose(uint32_t,const Fo3DoorCollisionPose&);
