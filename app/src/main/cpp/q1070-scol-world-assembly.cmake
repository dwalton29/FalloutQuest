# Q10.7: Fallout 3 REFRs may point at SCOL (Static Collection) records. A SCOL
# has no single MODL; it is authored as ONAM STAT references followed by DATA
# local transforms. Expand those parts into ordinary Fallout world placements
# before the existing NIF/collision paths consume them.

set(Q1070_OLD_BASE_STRUCT [=[
struct BaseRecordQ75 {
    uint32_t formId = 0;
    std::string recordType;
    std::string editorId;
    std::string modelPath;
};
]=])
set(Q1070_NEW_BASE_STRUCT [=[
struct ScolPartQ1070 {
    uint32_t baseFormId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
    float scale = 1.0f;
};

struct BaseRecordQ75 {
    uint32_t formId = 0;
    std::string recordType;
    std::string editorId;
    std::string modelPath;
    std::vector<ScolPartQ1070> scolParts;
};
]=])
string(REPLACE "${Q1070_OLD_BASE_STRUCT}" "${Q1070_NEW_BASE_STRUCT}"
       Q720_WORLDSPACE_SOURCE "${Q720_WORLDSPACE_SOURCE}")

set(Q1070_OLD_BASE_PARSE [=[
        BaseRecordQ75 base;
        base.formId = formId;
        base.recordType = FourCCQ75(header);
        WalkSubrecordsQ75(payload.data(), payload.size(),
                          [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "EDID", 4u) == 0 && base.editorId.empty()) {
                size_t len = 0u;
                while (len < size && bytes[len] != 0u) ++len;
                base.editorId.assign(reinterpret_cast<const char*>(bytes), len);
            } else if (std::memcmp(type, "MODL", 4u) == 0 && base.modelPath.empty()) {
                size_t len = 0u;
                while (len < size && bytes[len] != 0u) ++len;
                base.modelPath.assign(reinterpret_cast<const char*>(bytes), len);
            }
        });
]=])
set(Q1070_NEW_BASE_PARSE [=[
        BaseRecordQ75 base;
        base.formId = formId;
        base.recordType = FourCCQ75(header);
        uint32_t scolCurrentBaseQ1070 = 0u;
        WalkSubrecordsQ75(payload.data(), payload.size(),
                          [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "EDID", 4u) == 0 && base.editorId.empty()) {
                size_t len = 0u;
                while (len < size && bytes[len] != 0u) ++len;
                base.editorId.assign(reinterpret_cast<const char*>(bytes), len);
            } else if (std::memcmp(type, "MODL", 4u) == 0 && base.modelPath.empty()) {
                size_t len = 0u;
                while (len < size && bytes[len] != 0u) ++len;
                base.modelPath.assign(reinterpret_cast<const char*>(bytes), len);
            } else if (base.recordType == "SCOL" && std::memcmp(type, "ONAM", 4u) == 0 && size >= 4u) {
                scolCurrentBaseQ1070 = ReadLe32Q75(bytes);
            } else if (base.recordType == "SCOL" && std::memcmp(type, "DATA", 4u) == 0 &&
                       scolCurrentBaseQ1070 != 0u && size >= 28u) {
                // Fallout 3 SCOL DATA is one or more local placements:
                // position xyz, rotation xyz, scale (7 float32 values).
                for (uint32_t at = 0u; at + 28u <= size; at += 28u) {
                    ScolPartQ1070 part;
                    part.baseFormId = scolCurrentBaseQ1070;
                    part.x = ReadLeFloatQ75(bytes + at + 0u);
                    part.y = ReadLeFloatQ75(bytes + at + 4u);
                    part.z = ReadLeFloatQ75(bytes + at + 8u);
                    part.rx = ReadLeFloatQ75(bytes + at + 12u);
                    part.ry = ReadLeFloatQ75(bytes + at + 16u);
                    part.rz = ReadLeFloatQ75(bytes + at + 20u);
                    part.scale = ReadLeFloatQ75(bytes + at + 24u);
                    if (!(part.scale > 0.0001f && part.scale < 1000.0f)) part.scale = 1.0f;
                    base.scolParts.push_back(part);
                }
            }
        });
]=])
string(REPLACE "${Q1070_OLD_BASE_PARSE}" "${Q1070_NEW_BASE_PARSE}"
       Q720_WORLDSPACE_SOURCE "${Q720_WORLDSPACE_SOURCE}")

# Renderer-space Euler matrix matching q6a-native's RotateX -> RotateY -> RotateZ
# order. Compose SCOL local transforms exactly instead of adding Euler angles.
set(Q1070_ROTATION_HELPERS [=[
struct Mat3Q1070 {
    float m[9]{};
};

Mat3Q1070 RotationMatrixQ1070(float rx, float ry, float rz) {
    const float sx = std::sin(rx), cx = std::cos(rx);
    const float sy = std::sin(ry), cy = std::cos(ry);
    const float sz = std::sin(rz), cz = std::cos(rz);
    Mat3Q1070 r;
    r.m[0] = cy * cz;
    r.m[1] = cz * sy * sx - sz * cx;
    r.m[2] = cz * sy * cx + sz * sx;
    r.m[3] = cy * sz;
    r.m[4] = sz * sy * sx + cz * cx;
    r.m[5] = sz * sy * cx - cz * sx;
    r.m[6] = -sy;
    r.m[7] = cy * sx;
    r.m[8] = cy * cx;
    return r;
}

Mat3Q1070 MulMat3Q1070(const Mat3Q1070& a, const Mat3Q1070& b) {
    Mat3Q1070 out;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            out.m[row * 3 + col] =
                a.m[row * 3 + 0] * b.m[0 * 3 + col] +
                a.m[row * 3 + 1] * b.m[1 * 3 + col] +
                a.m[row * 3 + 2] * b.m[2 * 3 + col];
        }
    }
    return out;
}

void RotatePointQ1070(const Mat3Q1070& m, float x, float y, float z,
                      float& outX, float& outY, float& outZ) {
    outX = m.m[0] * x + m.m[1] * y + m.m[2] * z;
    outY = m.m[3] * x + m.m[4] * y + m.m[5] * z;
    outZ = m.m[6] * x + m.m[7] * y + m.m[8] * z;
}

void DecomposeRotationQ1070(const Mat3Q1070& m,
                            float& rx, float& ry, float& rz) {
    ry = std::asin(std::clamp(-m.m[6], -1.0f, 1.0f));
    const float cosY = std::cos(ry);
    if (std::fabs(cosY) > 1e-5f) {
        rx = std::atan2(m.m[7], m.m[8]);
        rz = std::atan2(m.m[3], m.m[0]);
    } else {
        rx = 0.0f;
        rz = std::atan2(-m.m[1], m.m[4]);
    }
}

]=])
string(REPLACE
    "} // namespace\n\nbool LoadFo3WorldspaceNeighborhoodQ75"
    "${Q1070_ROTATION_HELPERS}} // namespace\n\nbool LoadFo3WorldspaceNeighborhoodQ75"
    Q720_WORLDSPACE_SOURCE "${Q720_WORLDSPACE_SOURCE}")

# Resolve STAT bases referenced inside any SCOL selected by this exterior load.
set(Q1070_OLD_RESOLVE_BASES [=[
    std::unordered_map<uint32_t, BaseRecordQ75> bases;
    if (!ResolveBasesQ75(wanted, bases)) {
        Q75_LOGE("Q7.5 WORLDSPACE LOAD FAILED: worldspace=%08X reason=resolve-bases wanted=%zu",
                 worldspaceFormId, wanted.size());
        return false;
    }
]=])
set(Q1070_NEW_RESOLVE_BASES [=[
    std::unordered_map<uint32_t, BaseRecordQ75> bases;
    if (!ResolveBasesQ75(wanted, bases)) {
        Q75_LOGE("Q7.5 WORLDSPACE LOAD FAILED: worldspace=%08X reason=resolve-bases wanted=%zu",
                 worldspaceFormId, wanted.size());
        return false;
    }
    std::unordered_set<uint32_t> wantedWithScolQ1070 = wanted;
    size_t scolDefinitionsQ1070 = 0u;
    size_t scolDefinedPartsQ1070 = 0u;
    for (const auto& entry : bases) {
        const BaseRecordQ75& base = entry.second;
        if (base.recordType != "SCOL") continue;
        ++scolDefinitionsQ1070;
        scolDefinedPartsQ1070 += base.scolParts.size();
        for (const ScolPartQ1070& part : base.scolParts) {
            if (part.baseFormId != 0u) wantedWithScolQ1070.insert(part.baseFormId);
        }
    }
    if (wantedWithScolQ1070.size() > wanted.size() &&
        !ResolveBasesQ75(wantedWithScolQ1070, bases)) {
        Q75_LOGE("Q10.7 SCOL FAILED: stage=resolve-child-bases wanted=%zu expandedWanted=%zu",
                 wanted.size(), wantedWithScolQ1070.size());
        return false;
    }
]=])
string(REPLACE "${Q1070_OLD_RESOLVE_BASES}" "${Q1070_NEW_RESOLVE_BASES}"
       Q720_WORLDSPACE_SOURCE "${Q720_WORLDSPACE_SOURCE}")

set(Q1070_OLD_PLACEMENT_LOOP [=[
    std::unordered_map<std::string, size_t> baseTypes;
    std::unordered_set<std::string> uniqueModels;
    for (const RawPlacementQ75* p : active) {
        const auto it = bases.find(p->baseFormId);
        if (it == bases.end()) continue;
        const BaseRecordQ75& base = it->second;
        ++baseTypes[base.recordType.empty() ? "<none>" : base.recordType];
        if (base.modelPath.empty() || !EndsInNifQ75(base.modelPath)) continue;
        Fo3WorldPlacement world;
        world.refFormId = p->refFormId;
        world.baseFormId = p->baseFormId;
        world.baseRecordType = base.recordType;
        world.editorId = base.editorId;
        world.modelPath = base.modelPath;
        world.x = p->x;
        world.y = p->y;
        world.z = p->z;
        ConvertBethesdaRotationQ75(p->rx, p->ry, p->rz, world.rx, world.ry, world.rz);
        world.scale = p->scale;
        uniqueModels.insert(world.modelPath);
        outPlacements.push_back(std::move(world));
    }
]=])
set(Q1070_NEW_PLACEMENT_LOOP [=[
    std::unordered_map<std::string, size_t> baseTypes;
    std::unordered_set<std::string> uniqueModels;
    size_t scolPlacedRefsQ1070 = 0u;
    size_t scolExpandedPartsQ1070 = 0u;
    size_t scolUnresolvedPartsQ1070 = 0u;
    for (const RawPlacementQ75* p : active) {
        const auto it = bases.find(p->baseFormId);
        if (it == bases.end()) continue;
        const BaseRecordQ75& base = it->second;
        ++baseTypes[base.recordType.empty() ? "<none>" : base.recordType];

        if (base.recordType == "SCOL") {
            ++scolPlacedRefsQ1070;
            float parentRx = 0.0f, parentRy = 0.0f, parentRz = 0.0f;
            ConvertBethesdaRotationQ75(p->rx, p->ry, p->rz,
                                       parentRx, parentRy, parentRz);
            const Mat3Q1070 parentMatrix = RotationMatrixQ1070(parentRx, parentRy, parentRz);
            for (const ScolPartQ1070& part : base.scolParts) {
                const auto childIt = bases.find(part.baseFormId);
                if (childIt == bases.end() || childIt->second.modelPath.empty() ||
                    !EndsInNifQ75(childIt->second.modelPath)) {
                    ++scolUnresolvedPartsQ1070;
                    continue;
                }
                const BaseRecordQ75& childBase = childIt->second;
                float childRx = 0.0f, childRy = 0.0f, childRz = 0.0f;
                ConvertBethesdaRotationQ75(part.rx, part.ry, part.rz,
                                           childRx, childRy, childRz);
                const Mat3Q1070 childMatrix = RotationMatrixQ1070(childRx, childRy, childRz);
                const Mat3Q1070 worldMatrix = MulMat3Q1070(parentMatrix, childMatrix);

                float localWorldX = 0.0f, localWorldY = 0.0f, localWorldZ = 0.0f;
                RotatePointQ1070(parentMatrix,
                                 part.x * p->scale,
                                 part.y * p->scale,
                                 part.z * p->scale,
                                 localWorldX, localWorldY, localWorldZ);

                Fo3WorldPlacement world;
                world.refFormId = p->refFormId;
                world.baseFormId = part.baseFormId;
                world.baseRecordType = childBase.recordType;
                world.editorId = childBase.editorId;
                world.modelPath = childBase.modelPath;
                world.x = p->x + localWorldX;
                world.y = p->y + localWorldY;
                world.z = p->z + localWorldZ;
                DecomposeRotationQ1070(worldMatrix, world.rx, world.ry, world.rz);
                world.scale = p->scale * part.scale;
                uniqueModels.insert(world.modelPath);
                ++baseTypes[childBase.recordType.empty() ? "<none>" : childBase.recordType];
                outPlacements.push_back(std::move(world));
                ++scolExpandedPartsQ1070;
            }
            continue;
        }

        if (base.modelPath.empty() || !EndsInNifQ75(base.modelPath)) continue;
        Fo3WorldPlacement world;
        world.refFormId = p->refFormId;
        world.baseFormId = p->baseFormId;
        world.baseRecordType = base.recordType;
        world.editorId = base.editorId;
        world.modelPath = base.modelPath;
        world.x = p->x;
        world.y = p->y;
        world.z = p->z;
        ConvertBethesdaRotationQ75(p->rx, p->ry, p->rz, world.rx, world.ry, world.rz);
        world.scale = p->scale;
        uniqueModels.insert(world.modelPath);
        outPlacements.push_back(std::move(world));
    }
    Q75_LOGI("Q10.7 SCOL READY: definitions=%zu definedParts=%zu placedRefs=%zu expandedParts=%zu unresolvedParts=%zu source=Fallout3.esm/ONAM+DATA",
             scolDefinitionsQ1070, scolDefinedPartsQ1070, scolPlacedRefsQ1070,
             scolExpandedPartsQ1070, scolUnresolvedPartsQ1070);
]=])
string(REPLACE "${Q1070_OLD_PLACEMENT_LOOP}" "${Q1070_NEW_PLACEMENT_LOOP}"
       Q720_WORLDSPACE_SOURCE "${Q720_WORLDSPACE_SOURCE}")

string(FIND "${Q720_WORLDSPACE_SOURCE}" "Q10.7 SCOL READY" Q1070_SCOL_OK)
string(FIND "${Q720_WORLDSPACE_SOURCE}" "MulMat3Q1070" Q1070_SCOL_XFORM_OK)
if(Q1070_SCOL_OK LESS 0 OR Q1070_SCOL_XFORM_OK LESS 0)
    message(FATAL_ERROR "Q10.7 SCOL patch drifted: scol=${Q1070_SCOL_OK} xform=${Q1070_SCOL_XFORM_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-worldspace-q720.cpp" "${Q720_WORLDSPACE_SOURCE}")
