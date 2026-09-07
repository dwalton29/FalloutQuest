// Q7.18 exterior character-controller response.
//
// Q7.17 fixed the old persistent-overlap trap: tangent/outward movement can now
// escape authored contacts instead of being depenetrated back into them. The
// remaining ledge snag is different. Two low riser triangles can present two
// independent wall normals before the capsule centre has crossed far enough for
// FindStepGround to see the top, so clipping both normals reduces movement to
// zero. Q7.18 classifies the vertical extent of the actual contact triangles and
// performs a raised step-over for genuinely low blockers before treating them as
// a hard two-plane corner.

namespace {

struct WallContactQ718 {
    uint64_t surfaceKey = 0u;
    float nx = 0.0f;
    float nz = 0.0f;
    float push = 0.0f;
    float maxY = -1e30f;
    bool stairs = false;
};

constexpr float Q718_DEPTH_EPS = 0.0015f;
constexpr float Q718_DIRECTION_EPS = 0.00002f;
constexpr float Q718_TANGENT_NEW_CONTACT_MAX = 0.010f;
constexpr float Q718_LOW_BLOCKER_TOP_EPS = 0.012f;

uint64_t gSweepSlideLogsQ718 = 0u;
uint64_t gStepSweepLogsQ718 = 0u;
uint64_t gStepOverLogsQ718 = 0u;
uint64_t gPersistentEscapeLogsQ718 = 0u;
uint64_t gBlockedLogsQ718 = 0u;
bool gControllerReadyLoggedQ718 = false;

void GatherWallContactsQ718(float x, float z, float feetY,
                            std::vector<WallContactQ718>& out,
                            size_t* outRawTriangles = nullptr) {
    out.clear();
    if (outRawTriangles) *outRawTriangles = 0u;

    const float collisionRadius = CollisionRadiusQ712();
    std::unordered_map<uint64_t, size_t> slotBySurface;
    slotBySurface.reserve(8u);

    for (const CollisionTriangle& tri : gWorldTriangles) {
        WallContactQ714 contact;
        if (!ComputeWallContactQ714(tri, x, z, feetY, collisionRadius, contact)) continue;
        if (outRawTriangles) ++(*outRawTriangles);

        const auto found = slotBySurface.find(contact.surfaceKey);
        if (found == slotBySurface.end()) {
            const size_t slot = out.size();
            slotBySurface.emplace(contact.surfaceKey, slot);
            WallContactQ718 added;
            added.surfaceKey = contact.surfaceKey;
            added.nx = contact.nx;
            added.nz = contact.nz;
            added.push = contact.push;
            added.maxY = tri.maxY;
            added.stairs = tri.stairsQ714;
            out.push_back(added);
        } else {
            WallContactQ718& existing = out[found->second];
            existing.stairs = existing.stairs || tri.stairsQ714;
            existing.maxY = std::max(existing.maxY, tri.maxY);
            if (contact.push > existing.push) {
                existing.nx = contact.nx;
                existing.nz = contact.nz;
                existing.push = contact.push;
            }
        }
    }

    std::sort(out.begin(), out.end(), [](const WallContactQ718& a,
                                         const WallContactQ718& b) {
        return a.push > b.push;
    });
}

const WallContactQ718* FindSurfaceQ718(
    const std::vector<WallContactQ718>& contacts, uint64_t surfaceKey) {
    for (const WallContactQ718& contact : contacts) {
        if (contact.surfaceKey == surfaceKey) return &contact;
    }
    return nullptr;
}

bool CandidateAllowedQ718(float targetX, float targetZ, float feetY,
                          float moveX, float moveZ,
                          const std::vector<WallContactQ718>& baseline,
                          std::vector<WallContactQ718>& blockers,
                          size_t* outRawTriangles = nullptr,
                          size_t* outPersistent = nullptr) {
    std::vector<WallContactQ718> target;
    GatherWallContactsQ718(targetX, targetZ, feetY, target, outRawTriangles);
    blockers.clear();
    if (outPersistent) *outPersistent = 0u;

    for (const WallContactQ718& contact : target) {
        const WallContactQ718* previous = FindSurfaceQ718(baseline, contact.surfaceKey);
        const float into = moveX * contact.nx + moveZ * contact.nz;

        if (previous) {
            const float depthDelta = contact.push - previous->push;
            if (depthDelta <= Q718_DEPTH_EPS || into >= -Q718_DIRECTION_EPS) {
                if (outPersistent) ++(*outPersistent);
                continue;
            }
        } else if (into >= -Q718_DIRECTION_EPS &&
                   contact.push <= Q718_TANGENT_NEW_CONTACT_MAX) {
            if (outPersistent) ++(*outPersistent);
            continue;
        }

        blockers.push_back(contact);
    }
    return blockers.empty();
}

Vec2 ClipMoveAgainstContactsQ718(Vec2 move,
                                 const std::vector<WallContactQ718>& contacts,
                                 size_t& usedNormals) {
    struct Normal2 { float x = 0.0f, z = 0.0f; };
    std::vector<Normal2> normals;
    normals.reserve(std::min<size_t>(contacts.size(), 4u));

    for (const WallContactQ718& contact : contacts) {
        const float into = move.x * contact.nx + move.y * contact.nz;
        if (into >= -Q718_DIRECTION_EPS) continue;

        bool duplicate = false;
        for (const Normal2& n : normals) {
            if (n.x * contact.nx + n.z * contact.nz > 0.985f) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) normals.push_back({contact.nx, contact.nz});
        if (normals.size() >= 3u) break;
    }

    for (int pass = 0; pass < 3; ++pass) {
        for (const Normal2& n : normals) {
            const float into = move.x * n.x + move.y * n.z;
            if (into < 0.0f) {
                move.x -= n.x * into;
                move.y -= n.z * into;
            }
        }
    }

    usedNormals = normals.size();
    return move;
}

bool FindStepLandingQ718(float targetX, float targetZ,
                         float moveX, float moveZ,
                         float feetY, float& outLandingY) {
    const float length = std::sqrt(moveX * moveX + moveZ * moveZ);
    if (length < 0.00001f) return false;
    const float nx = moveX / length;
    const float nz = moveZ / length;
    const float radius = CollisionRadiusQ712();

    const std::array<float, 5> probes{
        0.0f, radius * 0.45f, radius * 0.75f, radius * 1.05f, radius * 1.35f
    };
    bool found = false;
    float lowest = std::numeric_limits<float>::max();
    for (float ahead : probes) {
        float candidateY = feetY;
        if (!FindStepGroundQ713(targetX + nx * ahead,
                                targetZ + nz * ahead,
                                feetY, candidateY)) continue;
        if (!found || candidateY < lowest) {
            lowest = candidateY;
            found = true;
        }
    }
    if (found) outLandingY = lowest;
    return found;
}

bool TryStepSweepQ718(float startX, float startZ,
                      float targetX, float targetZ,
                      float feetY, float& outFeetY,
                      size_t& blockingSurfaces) {
    const float moveX = targetX - startX;
    const float moveZ = targetZ - startZ;
    float landingY = feetY;
    if (!FindStepLandingQ718(targetX, targetZ, moveX, moveZ,
                             feetY, landingY)) return false;

    const float rise = landingY - feetY;
    if (rise <= STEP_MIN_RISE_Q78B ||
        rise > StepHeightQ78B() + PLAYER_SKIN) return false;

    const float raisedFeetY = feetY + StepHeightQ78B() + PLAYER_SKIN;
    if (HasOverheadBlock(startX, startZ, raisedFeetY) ||
        HasOverheadBlock(targetX, targetZ, landingY)) return false;

    std::vector<WallContactQ718> raisedBaseline;
    GatherWallContactsQ718(startX, startZ, raisedFeetY, raisedBaseline);
    std::vector<WallContactQ718> raisedBlockers;
    size_t raw = 0u;
    size_t persistent = 0u;
    CandidateAllowedQ718(targetX, targetZ, raisedFeetY,
                         moveX, moveZ, raisedBaseline,
                         raisedBlockers, &raw, &persistent);
    blockingSurfaces = raisedBlockers.size();
    if (!raisedBlockers.empty()) return false;

    outFeetY = landingY;
    if (gStepSweepLogsQ718 < 80u || (gResolveCounter % 360u) == 0u) {
        ++gStepSweepLogsQ718;
        Q6G_LOGI("Q7.18 STEP SWEEP: from=(%.3f %.3f) to=(%.3f %.3f) rise=%.3f raisedTestY=%.3f persistent=%zu maxStep=%.3f clear=1",
                 startX, startZ, targetX, targetZ,
                 rise, raisedFeetY, persistent, StepHeightQ78B());
    }
    return true;
}

bool TryLowBlockerStepOverQ718(float startX, float startZ,
                               float targetX, float targetZ,
                               float feetY,
                               const std::vector<WallContactQ718>& blockers,
                               float& outFeetY,
                               size_t& outRaisedBlockers) {
    outRaisedBlockers = 0u;
    if (blockers.empty()) return false;

    const float maxStep = StepHeightQ78B();
    float blockerTop = -1e30f;
    for (const WallContactQ718& blocker : blockers) {
        if (!std::isfinite(blocker.maxY)) return false;
        blockerTop = std::max(blockerTop, blocker.maxY);
    }

    const float riseToTop = blockerTop - feetY;
    if (riseToTop <= STEP_MIN_RISE_Q78B ||
        riseToTop > maxStep + PLAYER_SKIN + Q718_LOW_BLOCKER_TOP_EPS) {
        return false;
    }

    // Raise only as far as required to put the capsule bottom over the contact
    // triangles. This avoids the visible 30 cm pop of a max-height blind step.
    const float raisedFeetY = std::min(feetY + maxStep + PLAYER_SKIN,
                                      blockerTop + PLAYER_SKIN + Q718_LOW_BLOCKER_TOP_EPS);
    if (HasOverheadBlock(startX, startZ, raisedFeetY) ||
        HasOverheadBlock(targetX, targetZ, raisedFeetY)) return false;

    const float moveX = targetX - startX;
    const float moveZ = targetZ - startZ;
    std::vector<WallContactQ718> raisedBaseline;
    GatherWallContactsQ718(startX, startZ, raisedFeetY, raisedBaseline);
    std::vector<WallContactQ718> raisedBlockers;
    size_t raw = 0u;
    size_t persistent = 0u;
    CandidateAllowedQ718(targetX, targetZ, raisedFeetY,
                         moveX, moveZ, raisedBaseline,
                         raisedBlockers, &raw, &persistent);
    outRaisedBlockers = raisedBlockers.size();
    if (!raisedBlockers.empty()) return false;

    outFeetY = raisedFeetY;
    if (gStepOverLogsQ718 < 80u || (gResolveCounter % 240u) == 0u) {
        ++gStepOverLogsQ718;
        Q6G_LOGI("Q7.18 STEP OVER: from=(%.3f %.3f) to=(%.3f %.3f) blockerTop=%.3f rise=%.3f raisedTestY=%.3f blockers=%zu persistent=%zu clear=1",
                 startX, startZ, targetX, targetZ,
                 blockerTop, riseToTop, raisedFeetY,
                 blockers.size(), persistent);
    }
    return true;
}

bool MoveExteriorSubstepQ718(float& x, float& z, float& feetY,
                             float moveX, float moveZ,
                             bool& steppedThisFrame,
                             uint32_t& contactSurfaces) {
    if (moveX * moveX + moveZ * moveZ < 1e-10f) return true;

    std::vector<WallContactQ718> baseline;
    size_t baselineRaw = 0u;
    GatherWallContactsQ718(x, z, feetY, baseline, &baselineRaw);

    const float targetX = x + moveX;
    const float targetZ = z + moveZ;
    std::vector<WallContactQ718> blockers;
    size_t rawTriangles = 0u;
    size_t persistent = 0u;
    if (CandidateAllowedQ718(targetX, targetZ, feetY,
                             moveX, moveZ, baseline,
                             blockers, &rawTriangles, &persistent)) {
        if (!baseline.empty() &&
            (gPersistentEscapeLogsQ718 < 80u || (gResolveCounter % 240u) == 0u)) {
            ++gPersistentEscapeLogsQ718;
            Q6G_LOGI("Q7.18 OVERLAP ESCAPE: from=(%.3f %.3f) to=(%.3f %.3f) baselineSurfaces=%zu persistent=%zu targetRaw=%zu allowed=1",
                     x, z, targetX, targetZ,
                     baseline.size(), persistent, rawTriangles);
        }
        x = targetX;
        z = targetZ;
        return true;
    }
    contactSurfaces += static_cast<uint32_t>(blockers.size());

    size_t stepBlockers = 0u;
    float steppedFeetY = feetY;
    if (TryStepSweepQ718(x, z, targetX, targetZ,
                         feetY, steppedFeetY, stepBlockers)) {
        x = targetX;
        z = targetZ;
        feetY = steppedFeetY;
        steppedThisFrame = true;
        return true;
    }

    // Q7.18: if the normal landing probe cannot see the top yet, do not let two
    // low riser normals become a permanent corner. Prove the actual contact
    // triangles fit beneath maxStep, raise just over their top, and test the same
    // requested horizontal endpoint again at that raised height.
    size_t raisedStepBlockers = 0u;
    float stepOverFeetY = feetY;
    if (TryLowBlockerStepOverQ718(x, z, targetX, targetZ,
                                  feetY, blockers,
                                  stepOverFeetY, raisedStepBlockers)) {
        x = targetX;
        z = targetZ;
        feetY = stepOverFeetY;
        steppedThisFrame = true;
        return true;
    }

    Vec2 slide{moveX, moveZ};
    const float requestedLength = std::sqrt(moveX * moveX + moveZ * moveZ);
    size_t usedNormals = 0u;

    for (int iteration = 0; iteration < 3; ++iteration) {
        slide = ClipMoveAgainstContactsQ718(slide, blockers, usedNormals);
        const float slideLength2 = slide.x * slide.x + slide.y * slide.y;
        if (slideLength2 < 0.00000025f) break;

        const float slideX = x + slide.x;
        const float slideZ = z + slide.y;
        std::vector<WallContactQ718> slideBlockers;
        size_t slideRawTriangles = 0u;
        size_t slidePersistent = 0u;
        if (CandidateAllowedQ718(slideX, slideZ, feetY,
                                 slide.x, slide.y, baseline,
                                 slideBlockers, &slideRawTriangles,
                                 &slidePersistent)) {
            if (gSweepSlideLogsQ718 < 100u || (gResolveCounter % 240u) == 0u) {
                ++gSweepSlideLogsQ718;
                Q6G_LOGI("Q7.18 SWEEP SLIDE: from=(%.3f %.3f) requested=(%.3f %.3f) applied=(%.3f %.3f) blockers=%zu normals=%zu persistent=%zu",
                         x, z, moveX, moveZ, slide.x, slide.y,
                         blockers.size(), usedNormals, slidePersistent);
            }
            x = slideX;
            z = slideZ;
            return true;
        }

        float slideStepFeetY = feetY;
        size_t slideStepBlockers = 0u;
        if (TryStepSweepQ718(x, z, slideX, slideZ,
                             feetY, slideStepFeetY,
                             slideStepBlockers)) {
            x = slideX;
            z = slideZ;
            feetY = slideStepFeetY;
            steppedThisFrame = true;
            return true;
        }

        size_t slideRaisedBlockers = 0u;
        float slideStepOverY = feetY;
        if (TryLowBlockerStepOverQ718(x, z, slideX, slideZ,
                                      feetY, slideBlockers,
                                      slideStepOverY, slideRaisedBlockers)) {
            x = slideX;
            z = slideZ;
            feetY = slideStepOverY;
            steppedThisFrame = true;
            return true;
        }

        blockers.swap(slideBlockers);
        rawTriangles = slideRawTriangles;
        contactSurfaces += static_cast<uint32_t>(blockers.size());
    }

    if (gBlockedLogsQ718 < 80u || (gResolveCounter % 240u) == 0u) {
        ++gBlockedLogsQ718;
        const float appliedLength = std::sqrt(slide.x * slide.x + slide.y * slide.y);
        float blockerTop = -1e30f;
        for (const WallContactQ718& blocker : blockers) blockerTop = std::max(blockerTop, blocker.maxY);
        Q6G_LOGI("Q7.18 BLOCKED: pos=(%.3f %.3f) requestedLen=%.3f clippedLen=%.3f rawTriangles=%zu blockers=%zu normals=%zu stepBlockers=%zu raisedStepBlockers=%zu blockerTop=%.3f feetY=%.3f baseline=%zu",
                 x, z, requestedLength, appliedLength,
                 rawTriangles, blockers.size(), usedNormals,
                 stepBlockers, raisedStepBlockers,
                 blockerTop, feetY, baseline.size());
    }
    return false;
}

} // namespace

bool ResolveFo3PlayerMotionQ6G(float currentX, float currentZ,
                               float desiredX, float desiredZ,
                               float currentPlayerYOffset,
                               float* outX, float* outZ,
                               float* outPlayerYOffset) {
    if (!outX || !outZ || !outPlayerYOffset) return false;

    if (ConsumeFo3PlayerResetQ74()) {
        *outX = 0.0f;
        *outZ = 0.0f;
        *outPlayerYOffset = 0.0f;
        gSafeSpawnResolved = false;
        Q6G_LOGI("Q7.18 PLAYER RESET: bodyCenter=(0,0) source=XTEL newCollisionReady=%d",
                 gPlayerCollisionReady ? 1 : 0);
        return true;
    }
    if (!gPlayerCollisionReady || gWorldTriangles.empty()) return false;

    if (!gExteriorAllBhksQ78A || !gSafeSpawnResolved) {
        return ResolveFo3PlayerMotionLegacyQ716(currentX, currentZ,
                                                desiredX, desiredZ,
                                                currentPlayerYOffset,
                                                outX, outZ,
                                                outPlayerYOffset);
    }

    ++gResolveCounter;
    if (!gControllerReadyLoggedQ718) {
        gControllerReadyLoggedQ718 = true;
        Q6G_LOGI("Q7.18 CONTROLLER READY: mode=sweep-slide-step-over persistentOverlap=depth-aware depenetration=0 maxStep=%.3f depthEps=%.4f",
                 StepHeightQ78B(), Q718_DEPTH_EPS);
    }

    float x = currentX;
    float z = currentZ;
    float feetY = gCollisionFloorY + currentPlayerYOffset;

    const float requestedDx = desiredX - currentX;
    const float requestedDz = desiredZ - currentZ;
    const float distance = std::sqrt(requestedDx * requestedDx +
                                     requestedDz * requestedDz);
    const float maxSubstep = CollisionRadiusQ712() * 0.22f;
    const int steps = std::clamp(static_cast<int>(std::ceil(distance / maxSubstep)),
                                 1, 16);
    const float subDx = requestedDx / static_cast<float>(steps);
    const float subDz = requestedDz / static_cast<float>(steps);

    bool steppedThisFrame = false;
    uint32_t contactSurfaces = 0u;
    for (int step = 0; step < steps; ++step) {
        if (!MoveExteriorSubstepQ718(x, z, feetY,
                                     subDx, subDz,
                                     steppedThisFrame,
                                     contactSurfaces)) {
            break;
        }
    }

    float groundY = feetY;
    bool grounded = FindGround(x, z, feetY, groundY);
    const bool authoredGrounded = grounded;

    float terrainPlayerY = currentPlayerYOffset;
    const bool terrainGrounded = SampleFo3TerrainGroundQ77(x, z, &terrainPlayerY);
    if (terrainGrounded) {
        const float terrainGroundY = gCollisionFloorY + terrainPlayerY;
        if (!grounded || terrainGroundY > groundY) {
            groundY = terrainGroundY;
            grounded = true;
        }
        if (gTerrainGroundLogCountQ77 < 20u || (gResolveCounter % 360u) == 0u) {
            ++gTerrainGroundLogCountQ77;
            Q6G_LOGI("Q7.18 LAND GROUND: pos=(%.3f %.3f) terrainY=%.3f selectedGroundY=%.3f authoredGround=%d",
                     x, z, terrainGroundY, groundY,
                     authoredGrounded ? 1 : 0);
        }
    }

    // A blind step-over may temporarily put the capsule bottom above the old
    // floor before its centre has crossed onto the authored top. Preserve that
    // raised state for this frame; subsequent substeps/frames either find the
    // real top or naturally drop back to valid ground.
    if (steppedThisFrame && (!grounded || groundY < feetY - 0.05f)) {
        groundY = feetY;
        grounded = true;
    }

    float playerYOffset = feetY - gCollisionFloorY;
    if (grounded) playerYOffset = groundY - gCollisionFloorY;

    *outX = x;
    *outZ = z;
    *outPlayerYOffset = playerYOffset;

    const float correctionX = x - desiredX;
    const float correctionZ = z - desiredZ;
    const bool blocked = contactSurfaces > 0u ||
        correctionX * correctionX + correctionZ * correctionZ > 0.000004f;
    if (blocked && (gContactLogCount < 80u || (gResolveCounter % 120u) == 0u)) {
        ++gContactLogCount;
        Q6G_LOGI("Q7.18 CONTACT: desired=(%.3f %.3f) resolved=(%.3f %.3f) correction=(%.3f %.3f) blockers=%u stepped=%d grounded=%d playerY=%.3f",
                 desiredX, desiredZ, x, z,
                 correctionX, correctionZ, contactSurfaces,
                 steppedThisFrame ? 1 : 0,
                 grounded ? 1 : 0, playerYOffset);
    }
    return true;
}