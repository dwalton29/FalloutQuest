// Q7.16 exterior character-controller response.
//
// Q7.15 proved the Fallout 3 Havok geometry/subshape metadata is now decoded
// correctly. The remaining snagging came from the old response model: move the
// capsule into geometry and repeatedly depenetrate it. This exterior-only path
// instead treats penetration as a recovery case. Normal locomotion tests the
// requested endpoint, tries a raised step sweep for low obstacles, then clips
// the requested displacement against authored wall normals to slide.

namespace {

struct WallContactQ716 {
    uint64_t surfaceKey = 0u;
    float nx = 0.0f;
    float nz = 0.0f;
    float push = 0.0f;
    bool stairs = false;
};

uint64_t gSweepSlideLogsQ716 = 0u;
uint64_t gStepSweepLogsQ716 = 0u;
uint64_t gRecoveryLogsQ716 = 0u;
uint64_t gBlockedLogsQ716 = 0u;

void GatherWallContactsQ716(float x, float z, float feetY,
                            std::vector<WallContactQ716>& out,
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
            out.push_back(WallContactQ716{
                contact.surfaceKey, contact.nx, contact.nz, contact.push, tri.stairsQ714
            });
        } else {
            WallContactQ716& existing = out[found->second];
            existing.stairs = existing.stairs || tri.stairsQ714;
            if (contact.push > existing.push) {
                existing.nx = contact.nx;
                existing.nz = contact.nz;
                existing.push = contact.push;
            }
        }
    }

    std::sort(out.begin(), out.end(), [](const WallContactQ716& a,
                                         const WallContactQ716& b) {
        return a.push > b.push;
    });
}

bool PositionClearQ716(float x, float z, float feetY,
                       std::vector<WallContactQ716>& contacts,
                       size_t* rawTriangles = nullptr) {
    GatherWallContactsQ716(x, z, feetY, contacts, rawTriangles);
    return contacts.empty();
}

Vec2 ClipMoveAgainstContactsQ716(Vec2 move,
                                 const std::vector<WallContactQ716>& contacts,
                                 size_t& usedNormals) {
    struct Normal2 { float x = 0.0f, z = 0.0f; };
    std::vector<Normal2> normals;
    normals.reserve(std::min<size_t>(contacts.size(), 4u));

    for (const WallContactQ716& contact : contacts) {
        const float into = move.x * contact.nx + move.y * contact.nz;
        if (into >= -0.00001f) continue;

        bool duplicate = false;
        for (const Normal2& n : normals) {
            const float dot = n.x * contact.nx + n.z * contact.nz;
            if (dot > 0.985f) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) normals.push_back({contact.nx, contact.nz});
        if (normals.size() >= 3u) break;
    }

    // Sequential clipping is intentionally repeated. With two independent wall
    // planes the second projection can reintroduce a small component into the
    // first plane; a few Gauss-Seidel passes converge to the corner tangent.
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

bool FindStepLandingQ716(float targetX, float targetZ,
                         float moveX, float moveZ,
                         float feetY, float& outLandingY) {
    const float length = std::sqrt(moveX * moveX + moveZ * moveZ);
    if (length < 0.00001f) return false;
    const float nx = moveX / length;
    const float nz = moveZ / length;
    const float radius = CollisionRadiusQ712();

    const std::array<float, 3> probes{0.0f, radius * 0.70f, radius * 1.05f};
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

bool TryStepSweepQ716(float startX, float startZ,
                      float targetX, float targetZ,
                      float feetY, float& outFeetY,
                      size_t& blockingSurfaces) {
    float landingY = feetY;
    const float moveX = targetX - startX;
    const float moveZ = targetZ - startZ;
    if (!FindStepLandingQ716(targetX, targetZ, moveX, moveZ,
                             feetY, landingY)) return false;

    const float rise = landingY - feetY;
    if (rise <= STEP_MIN_RISE_Q78B ||
        rise > StepHeightQ78B() + PLAYER_SKIN) return false;

    // A real character controller performs the horizontal part of a step while
    // raised. Test the capsule at max step height so the vertical riser/pipe
    // below the feet cannot snag it, while a genuine tall wall still blocks.
    const float raisedFeetY = feetY + StepHeightQ78B() + PLAYER_SKIN;
    if (HasOverheadBlock(startX, startZ, raisedFeetY) ||
        HasOverheadBlock(targetX, targetZ, landingY)) return false;

    std::vector<WallContactQ716> raisedContacts;
    size_t raw = 0u;
    GatherWallContactsQ716(targetX, targetZ, raisedFeetY,
                           raisedContacts, &raw);
    blockingSurfaces = raisedContacts.size();
    if (!raisedContacts.empty()) return false;

    outFeetY = landingY;
    if (gStepSweepLogsQ716 < 40u || (gResolveCounter % 360u) == 0u) {
        ++gStepSweepLogsQ716;
        Q6G_LOGI("Q7.16 STEP SWEEP: from=(%.3f %.3f) to=(%.3f %.3f) rise=%.3f raisedTestY=%.3f maxStep=%.3f clear=1",
                 startX, startZ, targetX, targetZ,
                 rise, raisedFeetY, StepHeightQ78B());
    }
    return true;
}

bool MoveExteriorSubstepQ716(float& x, float& z, float& feetY,
                             float moveX, float moveZ,
                             bool& steppedThisFrame,
                             uint32_t& contactSurfaces) {
    if (moveX * moveX + moveZ * moveZ < 1e-10f) return true;

    const float targetX = x + moveX;
    const float targetZ = z + moveZ;
    std::vector<WallContactQ716> contacts;
    size_t rawTriangles = 0u;
    if (PositionClearQ716(targetX, targetZ, feetY,
                          contacts, &rawTriangles)) {
        x = targetX;
        z = targetZ;
        return true;
    }
    contactSurfaces += static_cast<uint32_t>(contacts.size());

    size_t stepBlockers = 0u;
    float steppedFeetY = feetY;
    if (TryStepSweepQ716(x, z, targetX, targetZ,
                         feetY, steppedFeetY, stepBlockers)) {
        x = targetX;
        z = targetZ;
        feetY = steppedFeetY;
        steppedThisFrame = true;
        return true;
    }

    Vec2 slide{moveX, moveZ};
    const float requestedLength = std::sqrt(moveX * moveX + moveZ * moveZ);
    size_t usedNormals = 0u;

    for (int iteration = 0; iteration < 3; ++iteration) {
        slide = ClipMoveAgainstContactsQ716(slide, contacts, usedNormals);
        const float slideLength2 = slide.x * slide.x + slide.y * slide.y;
        if (slideLength2 < 0.00000025f) break;

        const float slideX = x + slide.x;
        const float slideZ = z + slide.y;
        std::vector<WallContactQ716> slideContacts;
        size_t slideRawTriangles = 0u;
        if (PositionClearQ716(slideX, slideZ, feetY,
                              slideContacts, &slideRawTriangles)) {
            if (gSweepSlideLogsQ716 < 60u || (gResolveCounter % 240u) == 0u) {
                ++gSweepSlideLogsQ716;
                Q6G_LOGI("Q7.16 SWEEP SLIDE: from=(%.3f %.3f) requested=(%.3f %.3f) applied=(%.3f %.3f) rawTriangles=%zu surfaces=%zu normals=%zu",
                         x, z, moveX, moveZ, slide.x, slide.y,
                         rawTriangles, contacts.size(), usedNormals);
            }
            x = slideX;
            z = slideZ;
            return true;
        }

        float slideStepFeetY = feetY;
        size_t slideStepBlockers = 0u;
        if (TryStepSweepQ716(x, z, slideX, slideZ,
                             feetY, slideStepFeetY,
                             slideStepBlockers)) {
            x = slideX;
            z = slideZ;
            feetY = slideStepFeetY;
            steppedThisFrame = true;
            return true;
        }

        contacts.swap(slideContacts);
        rawTriangles = slideRawTriangles;
        contactSurfaces += static_cast<uint32_t>(contacts.size());
    }

    if (gBlockedLogsQ716 < 40u || (gResolveCounter % 240u) == 0u) {
        ++gBlockedLogsQ716;
        const float appliedLength = std::sqrt(slide.x * slide.x + slide.y * slide.y);
        Q6G_LOGI("Q7.16 BLOCKED: pos=(%.3f %.3f) requestedLen=%.3f clippedLen=%.3f rawTriangles=%zu surfaces=%zu normals=%zu stepBlockers=%zu",
                 x, z, requestedLength, appliedLength,
                 rawTriangles, contacts.size(), usedNormals, stepBlockers);
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

    // Consume the Q7.4 transition reset before choosing legacy/new response.
    if (ConsumeFo3PlayerResetQ74()) {
        *outX = 0.0f;
        *outZ = 0.0f;
        *outPlayerYOffset = 0.0f;
        gSafeSpawnResolved = false;
        Q6G_LOGI("Q7.16 PLAYER RESET: bodyCenter=(0,0) source=XTEL newCollisionReady=%d",
                 gPlayerCollisionReady ? 1 : 0);
        return true;
    }
    if (!gPlayerCollisionReady || gWorldTriangles.empty()) return false;

    // Keep interiors and the established safe-spawn bootstrap byte-for-byte on
    // the legacy path. Q7.16 only changes normal exterior locomotion.
    if (!gExteriorAllBhksQ78A || !gSafeSpawnResolved) {
        return ResolveFo3PlayerMotionLegacyQ716(currentX, currentZ,
                                                desiredX, desiredZ,
                                                currentPlayerYOffset,
                                                outX, outZ,
                                                outPlayerYOffset);
    }

    ++gResolveCounter;

    float x = currentX;
    float z = currentZ;
    float feetY = gCollisionFloorY + currentPlayerYOffset;

    // Recovery is allowed only when a frame starts already overlapping authored
    // geometry (for example physical room-scale head movement into a wall).
    // Normal stick movement below never calls depenetration on its target.
    std::vector<WallContactQ716> startingContacts;
    size_t recoveryRaw = 0u;
    GatherWallContactsQ716(x, z, feetY, startingContacts, &recoveryRaw);
    uint32_t recoveryContacts = 0u;
    if (!startingContacts.empty()) {
        const float beforeX = x;
        const float beforeZ = z;
        recoveryContacts = ResolveWallPenetrations(x, z, feetY);
        if (gRecoveryLogsQ716 < 24u || (gResolveCounter % 360u) == 0u) {
            ++gRecoveryLogsQ716;
            Q6G_LOGI("Q7.16 RECOVERY: from=(%.3f %.3f) to=(%.3f %.3f) rawTriangles=%zu surfaces=%zu depenetrationContacts=%u",
                     beforeX, beforeZ, x, z, recoveryRaw,
                     startingContacts.size(), recoveryContacts);
        }
    }

    const float requestedDx = desiredX - currentX;
    const float requestedDz = desiredZ - currentZ;
    const float distance = std::sqrt(requestedDx * requestedDx +
                                     requestedDz * requestedDz);
    const float maxSubstep = CollisionRadiusQ712() * 0.25f;
    const int steps = std::clamp(static_cast<int>(std::ceil(distance / maxSubstep)),
                                 1, 12);
    const float subDx = requestedDx / static_cast<float>(steps);
    const float subDz = requestedDz / static_cast<float>(steps);

    bool steppedThisFrame = false;
    uint32_t contactSurfaces = recoveryContacts;
    for (int step = 0; step < steps; ++step) {
        if (!MoveExteriorSubstepQ716(x, z, feetY,
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
        if (gTerrainGroundLogCountQ77 < 12u || (gResolveCounter % 360u) == 0u) {
            ++gTerrainGroundLogCountQ77;
            Q6G_LOGI("Q7.16 LAND GROUND: pos=(%.3f %.3f) terrainY=%.3f selectedGroundY=%.3f authoredGround=%d",
                     x, z, terrainGroundY, groundY,
                     authoredGrounded ? 1 : 0);
        }
    }

    // When the front of the capsule has found a valid step top but its centre
    // has not crossed the lip yet, do not immediately snap back to the lower
    // floor in the same frame. The next sweep continues across the riser and
    // normal grounding takes over as soon as the centre reaches the top.
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
    if (blocked && (gContactLogCount < 40u || (gResolveCounter % 120u) == 0u)) {
        ++gContactLogCount;
        Q6G_LOGI("Q7.16 CONTACT: desired=(%.3f %.3f) resolved=(%.3f %.3f) correction=(%.3f %.3f) surfaces=%u recovery=%u stepped=%d grounded=%d playerY=%.3f",
                 desiredX, desiredZ, x, z,
                 correctionX, correctionZ,
                 contactSurfaces, recoveryContacts,
                 steppedThisFrame ? 1 : 0,
                 grounded ? 1 : 0, playerYOffset);
    }
    return true;
}
