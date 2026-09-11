# Q16.0: authored Fallout 3 CELL traversal in VR.
#
#   aim an authored XTEL DOOR with the right controller
#   -> green A OPEN prompt above the right hand
#   -> right A queues the existing Q7.4 render-thread transition
#   -> submit one real Fallout3.esm LSCR loading-screen frame
#   -> load interior CELL or exterior WRLD neighborhood
#   -> keep the loading screen through the first completed destination frame
#
# Q15.15 output, Q15.16 sky and Q15.17 bloom are not modified.

# -----------------------------------------------------------------------------
# Transition translation unit: generic XTEL queue + interior CELL support.
# -----------------------------------------------------------------------------
if(NOT EXISTS "${Q720_CELL_SOURCE}")
    message(FATAL_ERROR "Q16.0 expected generated transition source at ${Q720_CELL_SOURCE}")
endif()
file(READ "${Q720_CELL_SOURCE}" Q1700_CELL_SOURCE)

set(Q1700_CELL_INCLUDE_OLD [==[
#include "fo3-transition-q74.h"
#include "fo3-terrain-q76.h"
]==])
set(Q1700_CELL_INCLUDE_NEW [==[
#include "fo3-transition-q74.h"
#include "fo3-terrain-q76.h"
#include "fo3-cell-traversal-q1700.h"
#include "fo3-loading-state-q1700.h"
]==])
string(FIND "${Q1700_CELL_SOURCE}" "${Q1700_CELL_INCLUDE_OLD}" Q1700_CELL_INCLUDE_POS)
if(Q1700_CELL_INCLUDE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find transition include block")
endif()
string(REPLACE "${Q1700_CELL_INCLUDE_OLD}" "${Q1700_CELL_INCLUDE_NEW}"
       Q1700_CELL_SOURCE "${Q1700_CELL_SOURCE}")

# Q7.5 intentionally rejected interiors. Preserve the proven exterior
# neighborhood path, then dispatch worldspace=0 destinations through the generic
# CELL loader already present in the included Q7 source.
set(Q1700_INTERIOR_OLD [==[
    Q71_LOGE("Q7.5 CELL LOAD FAILED: cell=%08X reason=no-worldspace-transition-context",
             cellFormId);
    outPlacements.clear();
    return false;
}
]==])
set(Q1700_INTERIOR_NEW [==[
    if (gPendingTransitionQ74.valid &&
        gPendingTransitionQ74.cellFormId == cellFormId &&
        gPendingTransitionQ74.worldspaceFormId == 0u) {
        ShutdownFo3TerrainRenderQ76();
        ActivateFo3TerrainGroundingQ77(0u, 0.0f, 0.0f, 0.0f,
                                       Q71_SCENE_FORWARD, Q71_FLOOR_Y,
                                       Q71_UNITS_PER_METRE);
        const bool loaded = LoadFo3CellPlacements(cellFormId, outPlacements);
        Q71_LOGI("Q16.0 INTERIOR CELL LOAD: cell=%08X placements=%zu loaded=%d source=Fallout3.esm",
                 cellFormId, outPlacements.size(), loaded ? 1 : 0);
        return loaded;
    }

    Q71_LOGE("Q16.0 CELL LOAD FAILED: cell=%08X reason=no-transition-context",
             cellFormId);
    outPlacements.clear();
    return false;
}
]==])
string(FIND "${Q1700_CELL_SOURCE}" "${Q1700_INTERIOR_OLD}" Q1700_INTERIOR_POS)
if(Q1700_INTERIOR_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find Q7.5 exterior-only rejection")
endif()
string(REPLACE "${Q1700_INTERIOR_OLD}" "${Q1700_INTERIOR_NEW}"
       Q1700_CELL_SOURCE "${Q1700_CELL_SOURCE}")

# Public generic queue. Q74ResolveOwner walks the real ESM hierarchy, so the
# destination REFR itself determines whether this is an interior CELL or WRLD.
set(Q1700_QUEUE [==[
bool QueueFo3DoorTransitionQ1700(uint32_t sourceDoorRef,
                                 uint32_t destinationDoorRef,
                                 float x, float y, float z,
                                 float rx, float ry, float rz) {
    if (destinationDoorRef == 0u) return false;
    if (gHasPendingTransitionQ74) {
        Q71_LOGI("Q16.0 DOOR QUEUE BUSY: sourceDoor=%08X destinationDoor=%08X",
                 sourceDoorRef, destinationDoorRef);
        return false;
    }

    Q74Owner owner;
    if (!Q74ResolveOwner(destinationDoorRef, owner) || !owner.valid) {
        Q71_LOGE("Q16.0 DOOR OWNER FAIL: sourceDoor=%08X destinationDoor=%08X",
                 sourceDoorRef, destinationDoorRef);
        return false;
    }

    gPendingTransitionQ74 = {};
    gPendingTransitionQ74.destinationDoorRef = destinationDoorRef;
    gPendingTransitionQ74.cellFormId = owner.cellFormId;
    gPendingTransitionQ74.worldspaceFormId = owner.worldspaceFormId;
    gPendingTransitionQ74.x = x;
    gPendingTransitionQ74.y = y;
    gPendingTransitionQ74.z = z;
    gPendingTransitionQ74.rx = rx;
    gPendingTransitionQ74.ry = ry;
    gPendingTransitionQ74.rz = rz;
    gPendingTransitionQ74.valid = true;
    gHasPendingTransitionQ74 = true;

    BeginFo3LoadingQ1700(owner.cellFormId, owner.worldspaceFormId);
    Q71_LOGI("Q16.0 DOOR QUEUED: sourceDoor=%08X destinationDoor=%08X cell=%08X worldspace=%08X kind=%s XTEL=(%.2f %.2f %.2f) R=(%.4f %.4f %.4f) loading=PRESENT",
             sourceDoorRef, destinationDoorRef, owner.cellFormId,
             owner.worldspaceFormId,
             owner.worldspaceFormId == 0u ? "INTERIOR" : "EXTERIOR",
             x, y, z, rx, ry, rz);
    return true;
}

]==])
set(Q1700_CONSUME_MARKER [==[
bool ConsumeFo3CellTransitionRequestQ74(Fo3CellTransitionRequestQ74& outRequest) {
]==])
string(FIND "${Q1700_CELL_SOURCE}" "${Q1700_CONSUME_MARKER}" Q1700_CONSUME_MARKER_POS)
if(Q1700_CONSUME_MARKER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find Q7.4 transition consume entry")
endif()
string(REPLACE "${Q1700_CONSUME_MARKER}"
       "${Q1700_QUEUE}${Q1700_CONSUME_MARKER}"
       Q1700_CELL_SOURCE "${Q1700_CELL_SOURCE}")

set(Q1700_CONSUME_OLD [==[
bool ConsumeFo3CellTransitionRequestQ74(Fo3CellTransitionRequestQ74& outRequest) {
    if (!gHasPendingTransitionQ74 || !gPendingTransitionQ74.valid) return false;
    outRequest = gPendingTransitionQ74;
]==])
set(Q1700_CONSUME_NEW [==[
bool ConsumeFo3CellTransitionRequestQ74(Fo3CellTransitionRequestQ74& outRequest) {
    if (!gHasPendingTransitionQ74 || !gPendingTransitionQ74.valid) return false;
    // Initial direct boot has no loading state and remains immediate. User door
    // transitions wait until one LSCR frame has actually reached xrEndFrame.
    if (ShouldDelayFo3TransitionConsumeQ1700()) return false;
    outRequest = gPendingTransitionQ74;
]==])
string(FIND "${Q1700_CELL_SOURCE}" "${Q1700_CONSUME_OLD}" Q1700_CONSUME_POS)
if(Q1700_CONSUME_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find transition consume body")
endif()
string(REPLACE "${Q1700_CONSUME_OLD}" "${Q1700_CONSUME_NEW}"
       Q1700_CELL_SOURCE "${Q1700_CELL_SOURCE}")

# An interior transition must also retire any exterior LAND left from the prior
# worldspace. Successful user transitions advance READY -> POST here; direct boot
# remains IDLE and is therefore unaffected.
set(Q1700_COMPLETE_OLD [==[
void CompleteFo3CellTransitionQ74(uint32_t cellFormId) {
    gCurrentCellQ74 = cellFormId;
    gPlayerResetPendingQ74 = true;

    bool terrainReady = false;
]==])
set(Q1700_COMPLETE_NEW [==[
void CompleteFo3CellTransitionQ74(uint32_t cellFormId) {
    gCurrentCellQ74 = cellFormId;
    gPlayerResetPendingQ74 = true;
    NotifyFo3TransitionCompleteQ1700();

    if (gPendingTransitionQ74.valid &&
        gPendingTransitionQ74.cellFormId == cellFormId &&
        gPendingTransitionQ74.worldspaceFormId == 0u) {
        ShutdownFo3TerrainRenderQ76();
        ActivateFo3TerrainGroundingQ77(0u, 0.0f, 0.0f, 0.0f,
                                       Q71_SCENE_FORWARD, Q71_FLOOR_Y,
                                       Q71_UNITS_PER_METRE);
    }

    bool terrainReady = false;
]==])
string(FIND "${Q1700_CELL_SOURCE}" "${Q1700_COMPLETE_OLD}" Q1700_COMPLETE_POS)
if(Q1700_COMPLETE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find transition completion body")
endif()
string(REPLACE "${Q1700_COMPLETE_OLD}" "${Q1700_COMPLETE_NEW}"
       Q1700_CELL_SOURCE "${Q1700_CELL_SOURCE}")

file(WRITE "${Q720_CELL_SOURCE}" "${Q1700_CELL_SOURCE}")

# -----------------------------------------------------------------------------
# Scene renderer: deterministic authored-DOOR aim query and queued activation.
# -----------------------------------------------------------------------------
string(PREPEND Q6H_NATIVE_SOURCE
       "#include \"fo3-cell-traversal-q1700.h\"\n#include \"fo3-loading-state-q1700.h\"\n#include \"fo3-loading-screen-q1700.h\"\n")

set(Q1700_DOOR_HELPERS [==[
bool QueryDoorInternalQ1700(float ox, float oy, float oz,
                            float dx, float dy, float dz,
                            Fo3DoorAimQ1700* outAim) {
    if (outAim) *outAim = {};
    if (!gSceneReady || IsFo3LoadingVisibleQ1700()) return false;
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-5f) return false;
    dx /= len; dy /= len; dz /= len;

    const GpuObject* hit = nullptr;
    float bestT = 3.0f;
    for (const GpuObject& object : gObjects) {
        if (object.baseRecordType != "DOOR" || !object.teleport.valid) continue;
        float t = 0.0f;
        if (RayAabbQ7(ox, oy, oz, dx, dy, dz, object, t) && t < bestT) {
            bestT = t;
            hit = &object;
        }
    }
    if (!hit) return false;

    if (outAim) {
        outAim->valid = true;
        outAim->sourceDoorRef = hit->refFormId;
        outAim->destinationDoorRef = hit->teleport.destinationDoorRefFormId;
        outAim->distance = bestT;
        outAim->x = hit->teleport.x;
        outAim->y = hit->teleport.y;
        outAim->z = hit->teleport.z;
        outAim->rx = hit->teleport.rx;
        outAim->ry = hit->teleport.ry;
        outAim->rz = hit->teleport.rz;
    }
    return true;
}

bool ActivateDoorInternalQ1700(float ox, float oy, float oz,
                               float dx, float dy, float dz) {
    Fo3DoorAimQ1700 aim;
    if (!QueryDoorInternalQ1700(ox, oy, oz, dx, dy, dz, &aim) || !aim.valid) {
        Q6H_LOGI("Q16.0 ACTIVATE MISS: maxDistance=3.0m");
        return false;
    }
    if (!QueueFo3DoorTransitionQ1700(aim.sourceDoorRef,
                                     aim.destinationDoorRef,
                                     aim.x, aim.y, aim.z,
                                     aim.rx, aim.ry, aim.rz)) {
        return false;
    }
    Q6H_LOGI("Q16.0 DOOR ACTIVATE: sourceDoor=%08X destinationDoor=%08X distance=%.2f input=RIGHT_A queue=Q7.4",
             aim.sourceDoorRef, aim.destinationDoorRef, aim.distance);
    return true;
}

]==])
set(Q1700_OLD_ACTIVATE_MARKER "bool ActivateDoorInternalQ7(float ox, float oy, float oz,")
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1700_OLD_ACTIVATE_MARKER}" Q1700_HELPER_POS)
if(Q1700_HELPER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find Q7 door ray helper tail")
endif()
string(REPLACE "${Q1700_OLD_ACTIVATE_MARKER}"
       "${Q1700_DOOR_HELPERS}${Q1700_OLD_ACTIVATE_MARKER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1700_PUBLIC_API [==[
bool QueryFo3DoorAimQ1700(float originX, float originY, float originZ,
                          float dirX, float dirY, float dirZ,
                          Fo3DoorAimQ1700* outAim) {
    return QueryDoorInternalQ1700(originX, originY, originZ,
                                  dirX, dirY, dirZ, outAim);
}

bool ActivateFo3DoorQ1700(float originX, float originY, float originZ,
                          float dirX, float dirY, float dirZ) {
    return ActivateDoorInternalQ1700(originX, originY, originZ,
                                     dirX, dirY, dirZ);
}

]==])
set(Q1700_GL_MARKER "#define glGenFramebuffers Q6HGenFramebuffers")
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1700_GL_MARKER}" Q1700_PUBLIC_POS)
if(Q1700_PUBLIC_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find renderer public bridge anchor")
endif()
string(REPLACE "${Q1700_GL_MARKER}"
       "${Q1700_PUBLIC_API}${Q1700_GL_MARKER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Keep the renderer's current-cell identity synchronized with the Q7.4 swap.
string(REPLACE
    "CompleteFo3CellTransitionQ74(request.cellFormId);"
    "CompleteFo3CellTransitionQ74(request.cellFormId);\n    gCurrentCellFormId = request.cellFormId;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Final OpenXR host: RIGHT A, door HUD, real loading image and frame gate.
# -----------------------------------------------------------------------------
set(Q1700_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1700_Q4_INPUT}")
    message(FATAL_ERROR "Q16.0 expected final OpenXR source at ${Q1700_Q4_INPUT}")
endif()
file(READ "${Q1700_Q4_INPUT}" Q1700_Q4_SOURCE)

string(REPLACE
    "#include \"fo3-door-runtime.h\""
    "#include \"fo3-door-runtime.h\"\n#include \"fo3-cell-traversal-q1700.h\"\n#include \"fo3-loading-state-q1700.h\"\n#include \"fo3-loading-screen-q1700.h\""
    Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")

# Oculus/Meta Touch right A click replaces the historical trigger activation.
string(REPLACE
    "/user/hand/right/input/trigger/click"
    "/user/hand/right/input/a/click"
    Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")
string(REPLACE
    "right trigger activate"
    "right A activate"
    Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")

# Q16.0 build label: reuse the vector display but change Q15.17 -> Q16.0.
string(REPLACE
    "constexpr unsigned q1600Digit5 = 0x6Du; // A F G C D"
    "constexpr unsigned q1600Digit5 = 0x7Du; // Q16.0: digit 6 = A F G E D C"
    Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")
set(Q1700_LABEL_TAIL_OLD [==[
        q1600Digit(q1600X, q1600Digit1);
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x07u); // 7 = A B C
]==])
set(Q1700_LABEL_TAIL_NEW [==[
        q1600Digit(q1600X, 0x3Fu); // Q16.0: 0 = A B C D E F
]==])
string(FIND "${Q1700_Q4_SOURCE}" "${Q1700_LABEL_TAIL_OLD}" Q1700_LABEL_TAIL_POS)
if(Q1700_LABEL_TAIL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find Q15.17 final label digits")
endif()
string(REPLACE "${Q1700_LABEL_TAIL_OLD}" "${Q1700_LABEL_TAIL_NEW}"
       Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")
string(REPLACE "Q15.17" "Q16.0" Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")

# Append fixed vector text A OPEN while q1600's vertex builder is still in scope.
set(Q1700_PROMPT_ANCHOR [==[
        versionVertexCount_ =
            static_cast<GLsizei>(vertices.size() / 3 - versionStartVertex_);
]==])
set(Q1700_PROMPT_GEOMETRY [==[
        versionVertexCount_ =
            static_cast<GLsizei>(vertices.size() / 3 - versionStartVertex_);

        interactionStartVertex_ = static_cast<GLint>(vertices.size() / 3);
        auto q1700Line = [&](float x0, float y0, float x1, float y1) {
            constexpr float z = 0.045f;
            vertices.insert(vertices.end(), {x0, y0, z, x1, y1, z});
        };
        constexpr float q1700Y = 0.078f;
        constexpr float q1700W = 0.018f;
        constexpr float q1700H = 0.032f;
        constexpr float q1700Gap = 0.005f;
        float q1700X = -0.073f;
        auto q1700Advance = [&]() { q1700X += q1700W + q1700Gap; };

        // A
        q1700Line(q1700X, q1700Y, q1700X + q1700W * 0.5f, q1700Y + q1700H);
        q1700Line(q1700X + q1700W * 0.5f, q1700Y + q1700H, q1700X + q1700W, q1700Y);
        q1700Line(q1700X + q1700W * 0.22f, q1700Y + q1700H * 0.48f,
                  q1700X + q1700W * 0.78f, q1700Y + q1700H * 0.48f);
        q1700Advance();
        q1700X += q1700Gap * 1.2f;

        // O
        q1700Line(q1700X, q1700Y, q1700X + q1700W, q1700Y);
        q1700Line(q1700X, q1700Y + q1700H, q1700X + q1700W, q1700Y + q1700H);
        q1700Line(q1700X, q1700Y, q1700X, q1700Y + q1700H);
        q1700Line(q1700X + q1700W, q1700Y, q1700X + q1700W, q1700Y + q1700H);
        q1700Advance();
        // P
        q1700Line(q1700X, q1700Y, q1700X, q1700Y + q1700H);
        q1700Line(q1700X, q1700Y + q1700H, q1700X + q1700W, q1700Y + q1700H);
        q1700Line(q1700X + q1700W, q1700Y + q1700H,
                  q1700X + q1700W, q1700Y + q1700H * 0.5f);
        q1700Line(q1700X, q1700Y + q1700H * 0.5f,
                  q1700X + q1700W, q1700Y + q1700H * 0.5f);
        q1700Advance();
        // E
        q1700Line(q1700X, q1700Y, q1700X, q1700Y + q1700H);
        q1700Line(q1700X, q1700Y + q1700H, q1700X + q1700W, q1700Y + q1700H);
        q1700Line(q1700X, q1700Y + q1700H * 0.5f,
                  q1700X + q1700W * 0.82f, q1700Y + q1700H * 0.5f);
        q1700Line(q1700X, q1700Y, q1700X + q1700W, q1700Y);
        q1700Advance();
        // N
        q1700Line(q1700X, q1700Y, q1700X, q1700Y + q1700H);
        q1700Line(q1700X + q1700W, q1700Y, q1700X + q1700W, q1700Y + q1700H);
        q1700Line(q1700X, q1700Y + q1700H, q1700X + q1700W, q1700Y);

        interactionVertexCount_ =
            static_cast<GLsizei>(vertices.size() / 3 - interactionStartVertex_);
]==])
string(FIND "${Q1700_Q4_SOURCE}" "${Q1700_PROMPT_ANCHOR}" Q1700_PROMPT_POS)
if(Q1700_PROMPT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find build-label geometry tail")
endif()
string(REPLACE "${Q1700_PROMPT_ANCHOR}" "${Q1700_PROMPT_GEOMETRY}"
       Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")

# Right-hand draw: Fallout/Pip-Boy green, visible through world geometry just like
# the development build tag. It appears only while a valid authored XTEL door is aimed.
set(Q1700_RIGHT_DRAW_OLD [==[
                } else {
                    SetMvpAndColor(mvp, 1.0f, 0.48f, 0.18f);
                    glDrawArrays(GL_TRIANGLES, controllerStartVertex_, controllerVertexCount_);
                }
]==])
set(Q1700_RIGHT_DRAW_NEW [==[
                } else {
                    SetMvpAndColor(mvp, 1.0f, 0.48f, 0.18f);
                    glDrawArrays(GL_TRIANGLES, controllerStartVertex_, controllerVertexCount_);
                    if (doorAimActiveQ1700_) {
                        glDepthFunc(GL_ALWAYS);
                        glLineWidth(2.0f);
                        SetMvpAndColor(mvp, 0.20f, 1.0f, 0.30f);
                        glDrawArrays(GL_LINES, interactionStartVertex_, interactionVertexCount_);
                        glDepthFunc(GL_LEQUAL);
                    }
                }
]==])
string(FIND "${Q1700_Q4_SOURCE}" "${Q1700_RIGHT_DRAW_OLD}" Q1700_RIGHT_DRAW_POS)
if(Q1700_RIGHT_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find right-controller draw block")
endif()
string(REPLACE "${Q1700_RIGHT_DRAW_OLD}" "${Q1700_RIGHT_DRAW_NEW}"
       Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")

string(REPLACE
    "    GLsizei versionVertexCount_{0};"
    "    GLsizei versionVertexCount_{0};\n    GLint interactionStartVertex_{0};\n    GLsizei interactionVertexCount_{0};\n    bool doorAimActiveQ1700_{false};"
    Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")

# Per-frame aim query shares exactly the same ray and 3m DOOR AABBs as activation.
set(Q1700_AIM_METHOD [==[
    void UpdateDoorAimQ1700() {
        doorAimActiveQ1700_ = false;
        if (IsFo3LoadingVisibleQ1700() || !handPoseValid_[1]) return;
        const XrPosef hand = ToVirtualPose(handLocalPoses_[1]);
        const Mat4 handMatrix = MatrixFromPose(hand);
        Fo3DoorAimQ1700 aim;
        doorAimActiveQ1700_ = QueryFo3DoorAimQ1700(
            hand.position.x, hand.position.y, hand.position.z,
            -handMatrix.m[8], -handMatrix.m[9], -handMatrix.m[10], &aim) && aim.valid;
    }

]==])
set(Q1700_RENDERFRAME_MARKER "    void RenderFrame() {")
string(FIND "${Q1700_Q4_SOURCE}" "${Q1700_RENDERFRAME_MARKER}" Q1700_RENDERFRAME_POS)
if(Q1700_RENDERFRAME_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find OpenXR RenderFrame entry")
endif()
string(REPLACE "${Q1700_RENDERFRAME_MARKER}"
       "${Q1700_AIM_METHOD}${Q1700_RENDERFRAME_MARKER}"
       Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")

# Keep locomotion frozen under the loading image and update the interaction HUD
# immediately before the existing activation edge logic.
set(Q1700_UPDATE_OLD [==[
                UpdatePlayer(views[0], frameState.predictedDisplayTime);
]==])
set(Q1700_UPDATE_NEW [==[
                if (!IsFo3LoadingVisibleQ1700()) {
                    UpdatePlayer(views[0], frameState.predictedDisplayTime);
                } else {
                    lastFrameTime_ = 0;
                }
                UpdateDoorAimQ1700();
]==])
string(FIND "${Q1700_Q4_SOURCE}" "${Q1700_UPDATE_OLD}" Q1700_UPDATE_POS)
if(Q1700_UPDATE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find player update before activation")
endif()
string(REPLACE "${Q1700_UPDATE_OLD}" "${Q1700_UPDATE_NEW}"
       Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")

# Only the new queued path is invoked from input; the historical synchronous Q7
# helper remains compiled for regression history but is no longer reachable here.
string(REPLACE "ActivateFo3DoorQ7(" "ActivateFo3DoorQ1700("
       Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")

# Build LSCR metadata during ordinary rendering, before the player ever presses a
# door. The 275MB ESM scan therefore cannot delay the first loading-image frame.
set(Q1700_BEGIN_OLD [==[
        if (!CheckXr(xrBeginFrame(session_, &beginInfo), "xrBeginFrame")) return;
]==])
set(Q1700_BEGIN_NEW [==[
        if (!CheckXr(xrBeginFrame(session_, &beginInfo), "xrBeginFrame")) return;
        PrepareFo3LoadingScreensQ1700();
]==])
string(FIND "${Q1700_Q4_SOURCE}" "${Q1700_BEGIN_OLD}" Q1700_BEGIN_POS)
if(Q1700_BEGIN_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find xrBeginFrame")
endif()
string(REPLACE "${Q1700_BEGIN_OLD}" "${Q1700_BEGIN_NEW}"
       Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")

# Draw Bethesda's selected LSCR after the Q15.17 final composite so loading art
# itself is not re-tonemapped or bloomed.
set(Q1700_COMPOSITE_OLD [==[
            Q1350CompositeEyePostQ1350(eyeIndex, framebuffer_, eye.width, eye.height);
            glFlush();
]==])
set(Q1700_COMPOSITE_NEW [==[
            Q1350CompositeEyePostQ1350(eyeIndex, framebuffer_, eye.width, eye.height);
            if (IsFo3LoadingVisibleQ1700()) {
                RenderFo3LoadingScreenQ1700(framebuffer_, eye.width, eye.height);
            }
            glFlush();
]==])
string(FIND "${Q1700_Q4_SOURCE}" "${Q1700_COMPOSITE_OLD}" Q1700_COMPOSITE_POS)
if(Q1700_COMPOSITE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find final Q15.17 eye composite")
endif()
string(REPLACE "${Q1700_COMPOSITE_OLD}" "${Q1700_COMPOSITE_NEW}"
       Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")

# The transition may consume only after a loading frame has successfully reached
# the OpenXR compositor. This is the deterministic PRESENT -> READY gate.
set(Q1700_END_OLD [==[
        CheckXr(xrEndFrame(session_, &endInfo), "xrEndFrame");
]==])
set(Q1700_END_NEW [==[
        const bool q1700FrameSubmitted =
            CheckXr(xrEndFrame(session_, &endInfo), "xrEndFrame");
        if (q1700FrameSubmitted) MarkFo3LoadingFramePresentedQ1700();
]==])
string(FIND "${Q1700_Q4_SOURCE}" "${Q1700_END_OLD}" Q1700_END_POS)
if(Q1700_END_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 could not find xrEndFrame")
endif()
string(REPLACE "${Q1700_END_OLD}" "${Q1700_END_NEW}"
       Q1700_Q4_SOURCE "${Q1700_Q4_SOURCE}")

# -----------------------------------------------------------------------------
# Hard guards: Q16 additions present and the successful visual baseline survives.
# -----------------------------------------------------------------------------
string(FIND "${Q1700_CELL_SOURCE}" "Q16.0 INTERIOR CELL LOAD:" Q1700_INTERIOR_OK)
string(FIND "${Q1700_CELL_SOURCE}" "QueueFo3DoorTransitionQ1700" Q1700_QUEUE_OK)
string(FIND "${Q1700_CELL_SOURCE}" "ShouldDelayFo3TransitionConsumeQ1700" Q1700_GATE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "QueryDoorInternalQ1700" Q1700_QUERY_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "colour = q1640PcOutput;" Q1700_Q1515_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q1670RenderPcBloomQ1670" Q1700_Q1517_OK)
string(FIND "${Q1700_Q4_SOURCE}" "RenderFo3PcSkyQ1660(skyMvp.m);" Q1700_Q1516_OK)
string(FIND "${Q1700_Q4_SOURCE}" "/user/hand/right/input/a/click" Q1700_A_OK)
string(FIND "${Q1700_Q4_SOURCE}" "/user/hand/right/input/trigger/click" Q1700_TRIGGER_OLD)
string(FIND "${Q1700_Q4_SOURCE}" "interactionStartVertex_" Q1700_PROMPT_OK)
string(FIND "${Q1700_Q4_SOURCE}" "RenderFo3LoadingScreenQ1700" Q1700_LSCR_OK)
string(FIND "${Q1700_Q4_SOURCE}" "text=Q16.0 anchor=left-hand" Q1700_LABEL_OK)
if(Q1700_INTERIOR_OK EQUAL -1 OR Q1700_QUEUE_OK EQUAL -1 OR
   Q1700_GATE_OK EQUAL -1 OR Q1700_QUERY_OK EQUAL -1 OR
   Q1700_Q1515_OK EQUAL -1 OR Q1700_Q1517_OK EQUAL -1 OR
   Q1700_Q1516_OK EQUAL -1 OR Q1700_A_OK EQUAL -1 OR
   NOT Q1700_TRIGGER_OLD EQUAL -1 OR Q1700_PROMPT_OK EQUAL -1 OR
   Q1700_LSCR_OK EQUAL -1 OR Q1700_LABEL_OK EQUAL -1)
    message(FATAL_ERROR
        "Q16.0 verification failed: interior=${Q1700_INTERIOR_OK} queue=${Q1700_QUEUE_OK} gate=${Q1700_GATE_OK} query=${Q1700_QUERY_OK} q1515=${Q1700_Q1515_OK} q1517=${Q1700_Q1517_OK} q1516=${Q1700_Q1516_OK} A=${Q1700_A_OK} oldTrigger=${Q1700_TRIGGER_OLD} prompt=${Q1700_PROMPT_OK} lscr=${Q1700_LSCR_OK} label=${Q1700_LABEL_OK}")
endif()

file(WRITE "${Q1700_Q4_INPUT}" "${Q1700_Q4_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q16.0 authored CELL traversal enabled: aim DOOR -> green right-hand A OPEN -> LSCR -> interior/exterior Q7.4 swap")
