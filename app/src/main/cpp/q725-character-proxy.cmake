# Q7.25: universal proxy-style exterior character controller.
#
# Q7.24 proved that local contact height matters, but model-specific exceptions
# are not a scalable character-controller architecture. Q7.25 stops relying on
# authored model names for traversal. It adds a continuous sampled capsule sweep,
# footprint support manifold, explicit up/forward/down step cast, and iterative
# contact-constraint projection for sliding.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q724.inc"
     Q725_CONTROLLER_SOURCE)
string(REPLACE "Q7.24" "Q7.25"
       Q725_CONTROLLER_SOURCE "${Q725_CONTROLLER_SOURCE}")

# Disable the Q7.23/Q7.24 model-name traversal exception. Keep the provenance
# plumbing for diagnostics, but Q7.25 traversal must not depend on asset names.
string(REPLACE
    "if (gExteriorAllBhksQ78A && tri.walkableModuleQ723) {"
    "if (false && gExteriorAllBhksQ78A && tri.walkableModuleQ723) {"
    Q725_CONTROLLER_SOURCE "${Q725_CONTROLLER_SOURCE}")

set(Q725_PROXY_SOURCE [=[
struct ProxyContactQ725 {
    uint64_t surfaceKey = 0u;
    float nx = 0.0f;
    float nz = 0.0f;
    float push = 0.0f;
    float contactY = 0.0f;
    float triangleTopY = 0.0f;
    uint32_t material = 0u;
    bool stairs = false;
    bool platform = false;
};

uint64_t gProxySweepLogsQ725 = 0u;
uint64_t gProxyStepLogsQ725 = 0u;
uint64_t gProxySlideLogsQ725 = 0u;
uint64_t gProxyBlockedLogsQ725 = 0u;
uint64_t gProxySupportLogsQ725 = 0u;

const ProxyContactQ725* FindProxySurfaceQ725(const std::vector<ProxyContactQ725>& contacts,
                                             uint64_t surfaceKey) {
    for (const ProxyContactQ725& contact : contacts) {
        if (contact.surfaceKey == surfaceKey) return &contact;
    }
    return nullptr;
}

void GatherProxyContactsQ725(float x, float z, float feetY,
                             std::vector<ProxyContactQ725>& out,
                             size_t* outRawTriangles = nullptr) {
    out.clear();
    if (outRawTriangles) *outRawTriangles = 0u;
    const float collisionRadius = CollisionRadiusQ712();
    const float radius2 = collisionRadius * collisionRadius;
    const float lowerY = feetY + collisionRadius;
    const float upperY = feetY + PLAYER_HEIGHT - collisionRadius;
    std::unordered_map<uint64_t, size_t> slotBySurface;
    slotBySurface.reserve(12u);

    for (const CollisionTriangle& tri : gWorldTriangles) {
        if (std::fabs(tri.normal.y) >= WalkableNormalThresholdQ714(tri)) continue;
        if (tri.maxY < feetY - PLAYER_SKIN || tri.minY > feetY + PLAYER_HEIGHT + PLAYER_SKIN) continue;
        if (x < tri.minX - collisionRadius || x > tri.maxX + collisionRadius ||
            z < tri.minZ - collisionRadius || z > tri.maxZ + collisionRadius) continue;

        const Vec3 segA{x, lowerY, z};
        const Vec3 segB{x, upperY, z};
        Vec3 capsulePoint;
        Vec3 trianglePoint;
        const float d2 = ClosestCapsuleSegmentTriangleQ719(segA, segB, tri,
                                                            capsulePoint, trianglePoint);
        if (!std::isfinite(d2) || d2 >= radius2) continue;
        if (outRawTriangles) ++(*outRawTriangles);

        const float distance = std::sqrt(std::max(d2, 0.0f));
        const Vec3 separation = Q719Sub(capsulePoint, trianglePoint);
        float nx = separation.x;
        float nz = separation.z;
        float horizontalLength = std::sqrt(nx * nx + nz * nz);
        if (horizontalLength > 1e-5f) {
            nx /= horizontalLength;
            nz /= horizontalLength;
        } else {
            nx = tri.normal.x;
            nz = tri.normal.z;
            horizontalLength = std::sqrt(nx * nx + nz * nz);
            if (horizontalLength < 1e-5f) continue;
            nx /= horizontalLength;
            nz /= horizontalLength;
            const float centroidX = (tri.a.x + tri.b.x + tri.c.x) / 3.0f;
            const float centroidZ = (tri.a.z + tri.b.z + tri.c.z) / 3.0f;
            if (nx * (x - centroidX) + nz * (z - centroidZ) < 0.0f) {
                nx = -nx;
                nz = -nz;
            }
        }

        ProxyContactQ725 candidate;
        candidate.surfaceKey = tri.surfaceKeyQ714;
        candidate.nx = nx;
        candidate.nz = nz;
        candidate.push = (collisionRadius - distance) + PLAYER_SKIN;
        candidate.contactY = trianglePoint.y;
        candidate.triangleTopY = tri.maxY;
        candidate.material = tri.havokMaterialQ714;
        candidate.stairs = tri.stairsQ714;
        candidate.platform = tri.platformQ714;

        const auto found = slotBySurface.find(candidate.surfaceKey);
        if (found == slotBySurface.end()) {
            const size_t slot = out.size();
            slotBySurface.emplace(candidate.surfaceKey, slot);
            out.push_back(candidate);
        } else {
            ProxyContactQ725& existing = out[found->second];
            existing.stairs = existing.stairs || candidate.stairs;
            existing.platform = existing.platform || candidate.platform;
            if (candidate.push > existing.push) existing = candidate;
        }
    }

    std::sort(out.begin(), out.end(), [](const ProxyContactQ725& a, const ProxyContactQ725& b) {
        return a.push > b.push;
    });
}

bool ProxyCandidateAllowedQ725(float targetX, float targetZ, float feetY,
                               float moveX, float moveZ,
                               const std::vector<ProxyContactQ725>& baseline,
                               std::vector<ProxyContactQ725>& blockers,
                               size_t* outRawTriangles = nullptr,
                               size_t* outPersistent = nullptr) {
    std::vector<ProxyContactQ725> target;
    GatherProxyContactsQ725(targetX, targetZ, feetY, target, outRawTriangles);
    blockers.clear();
    if (outPersistent) *outPersistent = 0u;

    for (const ProxyContactQ725& contact : target) {
        const ProxyContactQ725* previous = FindProxySurfaceQ725(baseline, contact.surfaceKey);
        const float into = moveX * contact.nx + moveZ * contact.nz;
        if (previous) {
            const float depthDelta = contact.push - previous->push;
            if (depthDelta <= 0.0025f || into >= -Q718_DIRECTION_EPS) {
                if (outPersistent) ++(*outPersistent);
                continue;
            }
        } else if (into >= -Q718_DIRECTION_EPS && contact.push <= 0.012f) {
            if (outPersistent) ++(*outPersistent);
            continue;
        }
        blockers.push_back(contact);
    }
    return blockers.empty();
}

bool SweepProxyCapsuleQ725(float startX, float startZ,
                           float targetX, float targetZ,
                           float feetY,
                           float& outX, float& outZ,
                           std::vector<ProxyContactQ725>& outBlockers,
                           size_t* outRawTriangles = nullptr,
                           size_t* outPersistent = nullptr) {
    outX = startX;
    outZ = startZ;
    outBlockers.clear();
    if (outRawTriangles) *outRawTriangles = 0u;
    if (outPersistent) *outPersistent = 0u;

    const float dx = targetX - startX;
    const float dz = targetZ - startZ;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length < 0.000001f) return true;

    const float sampleStep = std::max(0.012f, CollisionRadiusQ712() * 0.10f);
    const int samples = std::clamp(static_cast<int>(std::ceil(length / sampleStep)), 1, 12);
    std::vector<ProxyContactQ725> baseline;
    GatherProxyContactsQ725(startX, startZ, feetY, baseline);

    float previousX = startX;
    float previousZ = startZ;
    for (int i = 1; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float x = startX + dx * t;
        const float z = startZ + dz * t;
        const float stepX = x - previousX;
        const float stepZ = z - previousZ;
        std::vector<ProxyContactQ725> blockers;
        size_t raw = 0u, persistent = 0u;
        if (!ProxyCandidateAllowedQ725(x, z, feetY, stepX, stepZ,
                                       baseline, blockers, &raw, &persistent)) {
            outX = previousX;
            outZ = previousZ;
            outBlockers = std::move(blockers);
            if (outRawTriangles) *outRawTriangles = raw;
            if (outPersistent) *outPersistent = persistent;
            return false;
        }
        previousX = x;
        previousZ = z;
        GatherProxyContactsQ725(previousX, previousZ, feetY, baseline);
    }

    outX = targetX;
    outZ = targetZ;
    return true;
}

bool FindSupportFootprintQ725(float x, float z, float referenceFeetY,
                              float& groundY) {
    const float r = CollisionRadiusQ712() * 0.58f;
    const float d = r * 0.70710678f;
    const std::array<Vec2, 9> probes{{
        {0.0f, 0.0f},
        { r, 0.0f}, {-r, 0.0f}, {0.0f, r}, {0.0f, -r},
        { d, d}, { d,-d}, {-d, d}, {-d,-d}
    }};

    bool found = false;
    float bestY = referenceFeetY;
    float bestDistance = std::numeric_limits<float>::max();
    for (const CollisionTriangle& tri : gWorldTriangles) {
        if (std::fabs(tri.normal.y) < WalkableNormalThresholdQ714(tri)) continue;
        for (const Vec2& probe : probes) {
            const float px = x + probe.x;
            const float pz = z + probe.y;
            if (px < tri.minX - 0.01f || px > tri.maxX + 0.01f ||
                pz < tri.minZ - 0.01f || pz > tri.maxZ + 0.01f) continue;
            float u = 0.0f, v = 0.0f, w = 0.0f;
            if (!BarycentricXZ(tri, px, pz, u, v, w)) continue;
            const float y = tri.a.y * u + tri.b.y * v + tri.c.y * w;
            const float delta = y - referenceFeetY;
            if (delta > StepHeightQ78B() + PLAYER_SKIN || delta < -MAX_GROUND_DROP) continue;
            const float distance = std::fabs(delta);
            if (!found || distance < bestDistance - 0.0005f ||
                (std::fabs(distance - bestDistance) <= 0.0005f && y > bestY)) {
                found = true;
                bestDistance = distance;
                bestY = y;
            }
        }
    }
    if (found) groundY = bestY;
    return found;
}

bool TryProxyStepQ725(float startX, float startZ,
                      float targetX, float targetZ,
                      float feetY,
                      const std::vector<ProxyContactQ725>& blockers,
                      float& outFeetY) {
    if (blockers.empty()) return false;
    const float maxStep = StepHeightQ78B();

    float highestLocalContact = -1e30f;
    for (const ProxyContactQ725& blocker : blockers) {
        highestLocalContact = std::max(highestLocalContact, blocker.contactY);
    }
    const float contactRise = highestLocalContact - feetY;
    if (contactRise > maxStep + PLAYER_SKIN + 0.035f) return false;

    const float raisedFeetY = feetY + maxStep + PLAYER_SKIN;
    if (HasOverheadBlock(startX, startZ, raisedFeetY)) return false;

    float raisedX = startX, raisedZ = startZ;
    std::vector<ProxyContactQ725> raisedBlockers;
    size_t raisedRaw = 0u, raisedPersistent = 0u;
    if (!SweepProxyCapsuleQ725(startX, startZ, targetX, targetZ, raisedFeetY,
                               raisedX, raisedZ, raisedBlockers,
                               &raisedRaw, &raisedPersistent)) return false;

    float landingY = feetY;
    if (!FindSupportFootprintQ725(targetX, targetZ, feetY, landingY)) return false;
    const float rise = landingY - feetY;
    if (rise < -0.14f || rise > maxStep + PLAYER_SKIN) return false;
    if (HasOverheadBlock(targetX, targetZ, landingY)) return false;

    outFeetY = landingY;
    if (gProxyStepLogsQ725 < 120u || (gResolveCounter % 240u) == 0u) {
        ++gProxyStepLogsQ725;
        Q6G_LOGI("Q7.25 PROXY STEP: from=(%.3f %.3f) to=(%.3f %.3f) contactRise=%.3f landingRise=%.3f raisedRaw=%zu raisedPersistent=%zu blockers=%zu clear=1",
                 startX, startZ, targetX, targetZ, contactRise, rise,
                 raisedRaw, raisedPersistent, blockers.size());
    }
    return true;
}

Vec2 SolveProxyConstraintsQ725(Vec2 requested,
                               const std::vector<ProxyContactQ725>& contacts,
                               size_t& usedNormals) {
    struct Normal2Q725 { float x = 0.0f, z = 0.0f; };
    std::vector<Normal2Q725> normals;
    normals.reserve(std::min<size_t>(contacts.size(), 6u));
    for (const ProxyContactQ725& contact : contacts) {
        if (requested.x * contact.nx + requested.y * contact.nz >= -Q718_DIRECTION_EPS) continue;
        bool duplicate = false;
        for (const Normal2Q725& n : normals) {
            if (n.x * contact.nx + n.z * contact.nz > 0.985f) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) normals.push_back({contact.nx, contact.nz});
        if (normals.size() >= 4u) break;
    }

    Vec2 solved = requested;
    for (int pass = 0; pass < 5; ++pass) {
        bool changed = false;
        for (const Normal2Q725& n : normals) {
            const float into = solved.x * n.x + solved.y * n.z;
            if (into < 0.0f) {
                solved.x -= n.x * into;
                solved.y -= n.z * into;
                changed = true;
            }
        }
        if (!changed) break;
    }
    usedNormals = normals.size();
    return solved;
}

bool MoveExteriorSubstepQ725(float& x, float& z, float& feetY,
                             float moveX, float moveZ, bool& steppedThisFrame,
                             uint32_t& contactSurfaces) {
    if (moveX * moveX + moveZ * moveZ < 1e-10f) return true;

    const float startX = x;
    const float startZ = z;
    const float targetX = x + moveX;
    const float targetZ = z + moveZ;
    float sweptX = x, sweptZ = z;
    std::vector<ProxyContactQ725> blockers;
    size_t raw = 0u, persistent = 0u;

    if (SweepProxyCapsuleQ725(x, z, targetX, targetZ, feetY,
                              sweptX, sweptZ, blockers, &raw, &persistent)) {
        x = targetX;
        z = targetZ;
        return true;
    }

    // Preserve all collision-free progress up to first contact.
    x = sweptX;
    z = sweptZ;
    contactSurfaces += static_cast<uint32_t>(blockers.size());

    float stepFeetY = feetY;
    if (TryProxyStepQ725(x, z, targetX, targetZ, feetY, blockers, stepFeetY)) {
        x = targetX;
        z = targetZ;
        feetY = stepFeetY;
        steppedThisFrame = true;
        return true;
    }

    // Havok-style proxy response: solve the remaining desired velocity against
    // the whole contact manifold, then sweep the constrained displacement.
    for (int iteration = 0; iteration < 4; ++iteration) {
        const Vec2 remaining{targetX - x, targetZ - z};
        if (remaining.x * remaining.x + remaining.y * remaining.y < 0.00000025f) return true;

        size_t usedNormals = 0u;
        const Vec2 constrained = SolveProxyConstraintsQ725(remaining, blockers, usedNormals);
        if (constrained.x * constrained.x + constrained.y * constrained.y < 0.00000025f) break;

        const float slideTargetX = x + constrained.x;
        const float slideTargetZ = z + constrained.y;
        float slideX = x, slideZ = z;
        std::vector<ProxyContactQ725> slideBlockers;
        size_t slideRaw = 0u, slidePersistent = 0u;
        if (SweepProxyCapsuleQ725(x, z, slideTargetX, slideTargetZ, feetY,
                                  slideX, slideZ, slideBlockers,
                                  &slideRaw, &slidePersistent)) {
            if (gProxySlideLogsQ725 < 120u || (gResolveCounter % 240u) == 0u) {
                ++gProxySlideLogsQ725;
                Q6G_LOGI("Q7.25 PROXY SLIDE: from=(%.3f %.3f) requested=(%.3f %.3f) constrained=(%.3f %.3f) normals=%zu persistent=%zu",
                         x, z, remaining.x, remaining.y,
                         constrained.x, constrained.y, usedNormals, slidePersistent);
            }
            x = slideTargetX;
            z = slideTargetZ;
            return true;
        }

        x = slideX;
        z = slideZ;
        contactSurfaces += static_cast<uint32_t>(slideBlockers.size());

        float slideStepY = feetY;
        if (TryProxyStepQ725(x, z, slideTargetX, slideTargetZ,
                             feetY, slideBlockers, slideStepY)) {
            x = slideTargetX;
            z = slideTargetZ;
            feetY = slideStepY;
            steppedThisFrame = true;
            return true;
        }

        blockers.swap(slideBlockers);
        raw = slideRaw;
        persistent = slidePersistent;
    }

    if (gProxyBlockedLogsQ725 < 120u || (gResolveCounter % 240u) == 0u) {
        ++gProxyBlockedLogsQ725;
        float highestContact = -1e30f;
        uint64_t firstSurface = 0u;
        uint32_t firstMaterial = 0u;
        for (const ProxyContactQ725& blocker : blockers) {
            highestContact = std::max(highestContact, blocker.contactY);
            if (firstSurface == 0u) {
                firstSurface = blocker.surfaceKey;
                firstMaterial = blocker.material;
            }
        }
        const auto sourceIt = gSurfaceSourcesQ722.find(firstSurface);
        if (sourceIt != gSurfaceSourcesQ722.end()) {
            Q6G_LOGI("Q7.25 PROXY BLOCKER SOURCE: surface=%llu ref=%08X EDID=%s model=%s",
                     static_cast<unsigned long long>(firstSurface),
                     sourceIt->second.refFormId,
                     sourceIt->second.editorId.empty() ? "<none>" : sourceIt->second.editorId.c_str(),
                     sourceIt->second.modelPath.c_str());
        }
        Q6G_LOGI("Q7.25 PROXY BLOCKED: start=(%.3f %.3f) pos=(%.3f %.3f) target=(%.3f %.3f) blockers=%zu raw=%zu persistent=%zu contactRise=%.3f surface=%llu material=%u",
                 startX, startZ, x, z, targetX, targetZ,
                 blockers.size(), raw, persistent,
                 highestContact > -1e20f ? highestContact - feetY : -999.0f,
                 static_cast<unsigned long long>(firstSurface), firstMaterial);
    }
    return (x - startX) * (x - startX) + (z - startZ) * (z - startZ) > 0.00000025f;
}

]=])

# Insert the new proxy layer beside the old controller, then route exterior
# substeps through it. The legacy/interior controller remains untouched.
string(REPLACE
    "void LogTerrainCellsQ719() {"
    "${Q725_PROXY_SOURCE}void LogTerrainCellsQ719() {"
    Q725_CONTROLLER_SOURCE "${Q725_CONTROLLER_SOURCE}")

string(REPLACE
    "if (!MoveExteriorSubstepQ718(x, z, feetY, subDx, subDz,"
    "if (!MoveExteriorSubstepQ725(x, z, feetY, subDx, subDz,"
    Q725_CONTROLLER_SOURCE "${Q725_CONTROLLER_SOURCE}")

# Final grounding must use the capsule footprint as well; exact-centre-only
# support is one of the main reasons seams and narrow planks flicker between
# grounded and blocked states.
string(REPLACE
    "bool grounded = FindGround(x, z, feetY, groundY);"
    "bool grounded = FindSupportFootprintQ725(x, z, feetY, groundY);"
    Q725_CONTROLLER_SOURCE "${Q725_CONTROLLER_SOURCE}")

string(REPLACE
    "mode=3d-capsule-triangle+sweep-slide+raised-step+local-contact-endcaps persistentOverlap=depth-aware"
    "mode=proxy-manifold+swept-capsule+footprint-support+up-forward-down-step persistentOverlap=depth-aware"
    Q725_CONTROLLER_SOURCE "${Q725_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q725.inc"
     "${Q725_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q724.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q725.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
