cmake_minimum_required(VERSION 3.22.1)
project(falloutquest LANGUAGES C CXX)

find_package(OpenXR REQUIRED CONFIG)

# Q6C keeps the proven Q6A/Q6B OpenXR host but upgrades the scene from a fixed
# object count to a radius-selected room, centres the player inside the actual
# Bethesda-authored structure, and lets one NIF contribute multiple materials.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/q6a-native.cpp" Q6C_NATIVE_SOURCE)
string(REPLACE "Q6A" "Q6C" Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")

string(REPLACE
    "constexpr float SCENE_FORWARD = -3.20f;"
    "constexpr float SCENE_FORWARD = 0.00f;"
    Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")
string(REPLACE
    "constexpr size_t MAX_SCENE_OBJECTS = 8u;"
    "constexpr size_t MAX_SCENE_OBJECTS = 48u;"
    Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")
string(REPLACE
    "constexpr size_t MAX_MODEL_ATTEMPTS = 48u;"
    "constexpr size_t MAX_MODEL_ATTEMPTS = 512u;"
    Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")
string(REPLACE
    "constexpr float MAX_MODEL_EXTENT_UNITS = 500.0f;"
    "constexpr float MAX_MODEL_EXTENT_UNITS = 3000.0f;"
    Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")

# Anchor room discovery on the actual player bed, with the original chair as a
# deterministic fallback. This is only for finding the coherent room cluster;
# the final player origin below is calculated from the surrounding structure.
set(Q6C_OLD_SORT [=[
    std::stable_sort(placements.begin(), placements.end(),
                     [&](const Fo3WorldPlacement& a, const Fo3WorldPlacement& b) {
        const float adx = a.x - centroidX, ady = a.y - centroidY, adz = a.z - centroidZ;
        const float bdx = b.x - centroidX, bdy = b.y - centroidY, bdz = b.z - centroidZ;
        return adx * adx + ady * ady + adz * adz < bdx * bdx + bdy * bdy + bdz * bdz;
    });
]=])
set(Q6C_NEW_SORT [=[
    float anchorX = centroidX, anchorY = centroidY, anchorZ = centroidZ;
    const char* anchorSource = "centroid";
    for (const Fo3WorldPlacement& p : placements) {
        if (p.editorId == "HD01PlayerBed02") {
            anchorX = p.x; anchorY = p.y; anchorZ = p.z;
            anchorSource = "HD01PlayerBed02";
            break;
        }
    }
    if (std::string(anchorSource) == "centroid") {
        for (const Fo3WorldPlacement& p : placements) {
            if (p.editorId == "Chair03F") {
                anchorX = p.x; anchorY = p.y; anchorZ = p.z;
                anchorSource = "Chair03F";
                break;
            }
        }
    }

    auto roomScore = [&](const Fo3WorldPlacement& p) {
        if (p.editorId == "XMarker") return 1.0e30f;
        if (p.modelPath.rfind("Effects\\", 0) == 0 || p.modelPath.rfind("effects\\", 0) == 0)
            return 9.0e29f;
        const float dx = p.x - anchorX;
        const float dy = p.y - anchorY;
        const float dz = p.z - anchorZ;
        float score = dx * dx + dy * dy + dz * dz * 2.0f;
        const bool architecture =
            p.modelPath.find("Architecture\\Megaton\\interior\\ShackInteriors") != std::string::npos ||
            p.modelPath.find("architecture\\megaton\\interior\\shackinteriors") != std::string::npos;
        if (architecture) score *= 0.08f;
        if (p.baseRecordType == "DOOR") score *= 0.20f;
        if (p.baseRecordType == "FURN") score *= 0.45f;
        if (p.editorId == "HD01PlayerBed02") score = -3.0f;
        if (p.editorId == "Chair03F") score = -2.0f;
        return score;
    };

    Q6C_LOGI("Q6C ROOM ANCHOR: source=%s P=(%.1f %.1f %.1f)",
             anchorSource, anchorX, anchorY, anchorZ);
    std::stable_sort(placements.begin(), placements.end(),
                     [&](const Fo3WorldPlacement& a, const Fo3WorldPlacement& b) {
        return roomScore(a) < roomScore(b);
    });
]=])
string(REPLACE "${Q6C_OLD_SORT}" "${Q6C_NEW_SORT}"
       Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")

# One placed NIF may contain multiple visible geometry/material shapes. Build a
# CpuObject per material shape while applying the same ESM REFR transform to all
# of them, preserving the model exactly rather than discarding later shapes.
set(Q6C_OLD_BUILD [=[
bool BuildCpuObject(const Fo3WorldPlacement& placement, CpuObject& out) {
    Fo3StaticNifMesh mesh;
    if (!LoadFo3StaticNif(placement.modelPath, mesh)) return false;

    const float extent = MaxModelExtent(mesh) * placement.scale;
    if (!(extent >= 1.0f && extent <= MAX_MODEL_EXTENT_UNITS)) {
        Q6C_LOGW("Q6C SKIP SIZE: ref=%08X model=%s extent=%.1f",
                 placement.refFormId, placement.modelPath.c_str(), extent);
        return false;
    }

    const size_t vertexCount = mesh.positions.size() / 3u;
    if (mesh.normals.size() / 3u != vertexCount ||
        mesh.tangents.size() / 3u != vertexCount ||
        mesh.bitangents.size() / 3u != vertexCount ||
        mesh.texcoords.size() / 2u != vertexCount) return false;

    out = {};
    out.placement = placement;
    out.mesh = std::move(mesh);
    out.positionsGame.resize(vertexCount);
    out.normalsGame.resize(vertexCount);
    out.tangentsGame.resize(vertexCount);
    out.bitangentsGame.resize(vertexCount);

    for (size_t i = 0; i < vertexCount; ++i) {
        Vec3 position{
            out.mesh.positions[i * 3u] * placement.scale,
            out.mesh.positions[i * 3u + 1u] * placement.scale,
            out.mesh.positions[i * 3u + 2u] * placement.scale,
        };
        position = ApplyEsmRotation(position, placement);
        position.x += placement.x;
        position.y += placement.y;
        position.z += placement.z;
        out.positionsGame[i] = position;

        out.normalsGame[i] = Normalize(ApplyEsmRotation({
            out.mesh.normals[i * 3u], out.mesh.normals[i * 3u + 1u], out.mesh.normals[i * 3u + 2u]}, placement));
        out.tangentsGame[i] = Normalize(ApplyEsmRotation({
            out.mesh.tangents[i * 3u], out.mesh.tangents[i * 3u + 1u], out.mesh.tangents[i * 3u + 2u]}, placement));
        out.bitangentsGame[i] = Normalize(ApplyEsmRotation({
            out.mesh.bitangents[i * 3u], out.mesh.bitangents[i * 3u + 1u], out.mesh.bitangents[i * 3u + 2u]}, placement));
    }

    Q6C_LOGI("Q6C CPU OBJECT: ref=%08X base=%08X EDID=%s model=%s vertices=%zu triangles=%zu ESM=(%.1f %.1f %.1f / %.3f %.3f %.3f / %.3f)",
             placement.refFormId, placement.baseFormId,
             placement.editorId.empty() ? "<none>" : placement.editorId.c_str(),
             placement.modelPath.c_str(), vertexCount, out.mesh.indices.size() / 3u,
             placement.x, placement.y, placement.z,
             placement.rx, placement.ry, placement.rz, placement.scale);
    return true;
}
]=])
set(Q6C_NEW_BUILD [=[
bool BuildCpuObjects(const Fo3WorldPlacement& placement, std::vector<CpuObject>& outs) {
    outs.clear();
    std::vector<Fo3StaticNifMesh> meshes;
    if (!LoadFo3StaticNifMeshes(placement.modelPath, meshes)) return false;

    size_t shapeIndex = 0;
    for (Fo3StaticNifMesh& mesh : meshes) {
        const float extent = MaxModelExtent(mesh) * placement.scale;
        if (!(extent >= 1.0f && extent <= MAX_MODEL_EXTENT_UNITS)) {
            Q6C_LOGW("Q6C SKIP SHAPE SIZE: ref=%08X model=%s shape=%zu extent=%.1f",
                     placement.refFormId, placement.modelPath.c_str(), shapeIndex++, extent);
            continue;
        }

        const size_t vertexCount = mesh.positions.size() / 3u;
        if (mesh.normals.size() / 3u != vertexCount ||
            mesh.tangents.size() / 3u != vertexCount ||
            mesh.bitangents.size() / 3u != vertexCount ||
            mesh.texcoords.size() / 2u != vertexCount) {
            ++shapeIndex;
            continue;
        }

        CpuObject out;
        out.placement = placement;
        out.mesh = std::move(mesh);
        out.positionsGame.resize(vertexCount);
        out.normalsGame.resize(vertexCount);
        out.tangentsGame.resize(vertexCount);
        out.bitangentsGame.resize(vertexCount);

        for (size_t i = 0; i < vertexCount; ++i) {
            Vec3 position{
                out.mesh.positions[i * 3u] * placement.scale,
                out.mesh.positions[i * 3u + 1u] * placement.scale,
                out.mesh.positions[i * 3u + 2u] * placement.scale,
            };
            position = ApplyEsmRotation(position, placement);
            position.x += placement.x;
            position.y += placement.y;
            position.z += placement.z;
            out.positionsGame[i] = position;

            out.normalsGame[i] = Normalize(ApplyEsmRotation({
                out.mesh.normals[i * 3u], out.mesh.normals[i * 3u + 1u], out.mesh.normals[i * 3u + 2u]}, placement));
            out.tangentsGame[i] = Normalize(ApplyEsmRotation({
                out.mesh.tangents[i * 3u], out.mesh.tangents[i * 3u + 1u], out.mesh.tangents[i * 3u + 2u]}, placement));
            out.bitangentsGame[i] = Normalize(ApplyEsmRotation({
                out.mesh.bitangents[i * 3u], out.mesh.bitangents[i * 3u + 1u], out.mesh.bitangents[i * 3u + 2u]}, placement));
        }

        Q6C_LOGI("Q6C CPU SHAPE: ref=%08X EDID=%s shape=%zu/%zu vertices=%zu triangles=%zu diffuse=%s",
                 placement.refFormId,
                 placement.editorId.empty() ? "<none>" : placement.editorId.c_str(),
                 shapeIndex + 1u, meshes.size(), vertexCount, out.mesh.indices.size() / 3u,
                 out.mesh.diffuseTexturePath.empty() ? "<none>" : out.mesh.diffuseTexturePath.c_str());
        outs.push_back(std::move(out));
        ++shapeIndex;
    }
    return !outs.empty();
}
]=])
string(REPLACE "${Q6C_OLD_BUILD}" "${Q6C_NEW_BUILD}"
       Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")

# Radius is the primary completeness rule. Structural pieces get a generous
# envelope so walls/floor/ceiling/roof pieces survive; props use a tighter one.
# MAX_SCENE_OBJECTS is now only a safety cap on placed REFRs, not the room shape.
set(Q6C_OLD_SELECT [=[
    std::vector<CpuObject> selected;
    selected.reserve(MAX_SCENE_OBJECTS);
    size_t attempts = 0;
    for (const Fo3WorldPlacement& placement : placements) {
        if (selected.size() >= MAX_SCENE_OBJECTS || attempts >= MAX_MODEL_ATTEMPTS) break;
        ++attempts;
        CpuObject object;
        if (BuildCpuObject(placement, object)) selected.push_back(std::move(object));
    }
]=])
set(Q6C_NEW_SELECT [=[
    std::vector<CpuObject> selected;
    selected.reserve(MAX_SCENE_OBJECTS * 2u);
    size_t attempts = 0;
    size_t selectedPlacements = 0;
    size_t filteredEffects = 0;
    for (const Fo3WorldPlacement& placement : placements) {
        if (selectedPlacements >= MAX_SCENE_OBJECTS || attempts >= MAX_MODEL_ATTEMPTS) break;
        ++attempts;

        if (placement.editorId == "XMarker" ||
            placement.modelPath.rfind("Effects\\", 0) == 0 ||
            placement.modelPath.rfind("effects\\", 0) == 0) {
            ++filteredEffects;
            continue;
        }

        const float dx = placement.x - anchorX;
        const float dy = placement.y - anchorY;
        const float dz = placement.z - anchorZ;
        const float horizontal2 = dx * dx + dy * dy;
        const bool architecture =
            placement.modelPath.find("Architecture\\Megaton\\interior\\ShackInteriors") != std::string::npos ||
            placement.modelPath.find("architecture\\megaton\\interior\\shackinteriors") != std::string::npos;

        const bool inRoomRadius = architecture
            ? (horizontal2 <= 950.0f * 950.0f && std::fabs(dz) <= 700.0f)
            : (horizontal2 <= 575.0f * 575.0f && std::fabs(dz) <= 450.0f);
        if (!inRoomRadius) continue;

        std::vector<CpuObject> parts;
        if (BuildCpuObjects(placement, parts)) {
            ++selectedPlacements;
            for (CpuObject& part : parts) selected.push_back(std::move(part));
        }
    }
    Q6C_LOGI("Q6C ROOM SELECT: placements=%zu drawShapes=%zu attempts=%zu filteredMarkersEffects=%zu radiusDriven=1",
             selectedPlacements, selected.size(), attempts, filteredEffects);
]=])
string(REPLACE "${Q6C_OLD_SELECT}" "${Q6C_NEW_SELECT}"
       Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")

# Centre the headset on the horizontal envelope of actual Megaton interior
# structure refs (walls/corners/doors), not on furniture/prop geometry. This
# makes VR origin the room centre. Floor height still comes from the assembled
# geometry so the existing Quest standing-height convention remains intact.
set(Q6C_OLD_CENTER [=[
    const float centerX = (minimum.x + maximum.x) * 0.5f;
    const float centerY = (minimum.y + maximum.y) * 0.5f;
    const float floorZ = minimum.z;
]=])
set(Q6C_NEW_CENTER [=[
    float structureMinX = 1e30f, structureMinY = 1e30f;
    float structureMaxX = -1e30f, structureMaxY = -1e30f;
    size_t structureRefs = 0;
    for (const CpuObject& object : selected) {
        const std::string& path = object.placement.modelPath;
        const bool architecture =
            path.find("Architecture\\Megaton\\interior\\ShackInteriors") != std::string::npos ||
            path.find("architecture\\megaton\\interior\\shackinteriors") != std::string::npos;
        if (!architecture) continue;
        structureMinX = std::min(structureMinX, object.placement.x);
        structureMaxX = std::max(structureMaxX, object.placement.x);
        structureMinY = std::min(structureMinY, object.placement.y);
        structureMaxY = std::max(structureMaxY, object.placement.y);
        ++structureRefs;
    }

    const float centerX = structureRefs > 0u
        ? (structureMinX + structureMaxX) * 0.5f
        : (minimum.x + maximum.x) * 0.5f;
    const float centerY = structureRefs > 0u
        ? (structureMinY + structureMaxY) * 0.5f
        : (minimum.y + maximum.y) * 0.5f;
    const float floorZ = minimum.z;
    Q6C_LOGI("Q6C PLAYER ORIGIN: roomCenter=(%.1f %.1f) floorZ=%.1f structureShapes=%zu VR=(0,standing,0)",
             centerX, centerY, floorZ, structureRefs);
]=])
string(REPLACE "${Q6C_OLD_CENTER}" "${Q6C_NEW_CENTER}"
       Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")

# Avoid the two helper-name collisions with the included proven Q4 renderer.
string(REPLACE
    "GLuint CompileShader(GLenum type, const char* source)"
    "GLuint CompileQ6CShader(GLenum type, const char* source)"
    Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")
string(REPLACE
    "GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSource);"
    "GLuint vs = CompileQ6CShader(GL_VERTEX_SHADER, vertexSource);"
    Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")
string(REPLACE
    "GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);"
    "GLuint fs = CompileQ6CShader(GL_FRAGMENT_SHADER, fragmentSource);"
    Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")
string(REPLACE
    "GLuint CreateProgram()"
    "GLuint CreateQ6CProgram()"
    Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")
string(REPLACE
    "gProgram = CreateProgram();"
    "gProgram = CreateQ6CProgram();"
    Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")

# Q6E overlays Bethesda-authored bhk collision over the working Q6D scene.
# The overlay is verification-only and may fail per asset without failing scene
# initialization. It uses the exact room centre/floor transform used by visuals.
string(REPLACE
    "#include \"fo3-texture-bsa.h\""
    "#include \"fo3-texture-bsa.h\"\n#include \"fo3-collision-overlay.h\""
    Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")

set(Q6E_OLD_PROGRAM [=[
    gProgram = CreateQ6CProgram();
]=])
set(Q6E_NEW_PROGRAM [=[
    std::vector<Fo3WorldPlacement> q6eCollisionPlacements;
    q6eCollisionPlacements.reserve(selected.size());
    for (const CpuObject& object : selected) {
        q6eCollisionPlacements.push_back(object.placement);
    }
    InitializeFo3CollisionOverlay(q6eCollisionPlacements,
                                  centerX, centerY, floorZ,
                                  SCENE_FORWARD, FLOOR_Y,
                                  FO3_UNITS_PER_METRE);

    gProgram = CreateQ6CProgram();
]=])
string(REPLACE "${Q6E_OLD_PROGRAM}" "${Q6E_NEW_PROGRAM}"
       Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")

set(Q6E_OLD_RENDER_RESTORE [=[
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
]=])
set(Q6E_NEW_RENDER_RESTORE [=[
    RenderFo3CollisionOverlay(mvp);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
]=])
string(REPLACE "${Q6E_OLD_RENDER_RESTORE}" "${Q6E_NEW_RENDER_RESTORE}"
       Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")

set(Q6E_OLD_CLEANUP [=[
    glDeleteFramebuffers(n, framebuffers);
    for (GpuObject& object : gObjects) {
]=])
set(Q6E_NEW_CLEANUP [=[
    glDeleteFramebuffers(n, framebuffers);
    ShutdownFo3CollisionOverlay();
    for (GpuObject& object : gObjects) {
]=])
string(REPLACE "${Q6E_OLD_CLEANUP}" "${Q6E_NEW_CLEANUP}"
       Q6C_NATIVE_SOURCE "${Q6C_NATIVE_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6c-native-generated.cpp" "${Q6C_NATIVE_SOURCE}")

# Generate the multi-shape NIF loader from the already-proven parser. The only
# semantic change is that it keeps every supported visible shape instead of
# returning immediately after the first one.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-static-nif.cpp" Q6C_NIF_SOURCE)
string(REPLACE "Q6A" "Q6C" Q6C_NIF_SOURCE "${Q6C_NIF_SOURCE}")
set(Q6C_OLD_LOADER [=[
bool LoadFo3StaticNif(const std::string& modelPath, Fo3StaticNifMesh& outMesh) {
    outMesh = {};
    outMesh.modelPath = modelPath;

    std::vector<uint8_t> nif;
    std::string resolved;
    if (!LoadFalloutMeshFile(modelPath, nif, &resolved)) return false;
    if (nif.empty() || nif.size() > MAX_NIF_BYTES) return false;

    NifHeader header;
    if (!ParseHeader(nif, header)) {
        Q6C_LOGW("Q6C NIF UNSUPPORTED HEADER: %s", resolved.c_str());
        return false;
    }

    NifTransform rootTransform;
    std::vector<uint32_t> rootChildren;
    bool haveRoot = false;
    for (uint32_t block = 0; block < header.numBlocks; ++block) {
        const std::string& type = BlockType(header, block);
        if (type != "NiNode" && type != "BSFadeNode") continue;
        std::vector<uint32_t> children;
        NifTransform transform;
        if (ParseNode(BlockData(nif, header, block), header.blockSizes[block],
                      header, transform, children)) {
            rootTransform = transform;
            rootChildren = std::move(children);
            haveRoot = true;
            break;
        }
    }

    for (uint32_t block = 0; block < header.numBlocks; ++block) {
        const std::string& type = BlockType(header, block);
        if (type != "NiTriStrips" && type != "NiTriShape") continue;

        ShapeObject shape;
        shape.block = block;
        if (!ParseShapeObject(BlockData(nif, header, block), header.blockSizes[block],
                              header, shape)) continue;

        const bool directRootChild = haveRoot &&
            std::find(rootChildren.begin(), rootChildren.end(), block) != rootChildren.end();
        Fo3StaticNifMesh candidate;
        if (!TryLoadShape(nif, header, shape,
                          directRootChild ? &rootTransform : nullptr,
                          candidate)) continue;

        candidate.modelPath = resolved;
        outMesh = std::move(candidate);
        Q6C_LOGI("Q6C NIF READY: model=%s shape=%u type=%s data=%u vertices=%zu triangles=%zu diffuse=%s normal=%s rootTransform=%d",
                 resolved.c_str(), block, type.c_str(), shape.dataRef,
                 outMesh.positions.size() / 3u, outMesh.indices.size() / 3u,
                 outMesh.diffuseTexturePath.empty() ? "<none>" : outMesh.diffuseTexturePath.c_str(),
                 outMesh.normalTexturePath.empty() ? "<none>" : outMesh.normalTexturePath.c_str(),
                 directRootChild ? 1 : 0);
        return true;
    }

    Q6C_LOGW("Q6C NIF NO SUPPORTED STATIC SHAPE: %s blocks=%u", resolved.c_str(), header.numBlocks);
    return false;
}
]=])
set(Q6C_NEW_LOADER [=[
bool LoadFo3StaticNifMeshes(const std::string& modelPath,
                            std::vector<Fo3StaticNifMesh>& outMeshes) {
    outMeshes.clear();

    std::vector<uint8_t> nif;
    std::string resolved;
    if (!LoadFalloutMeshFile(modelPath, nif, &resolved)) return false;
    if (nif.empty() || nif.size() > MAX_NIF_BYTES) return false;

    NifHeader header;
    if (!ParseHeader(nif, header)) {
        Q6C_LOGW("Q6C NIF UNSUPPORTED HEADER: %s", resolved.c_str());
        return false;
    }

    NifTransform rootTransform;
    std::vector<uint32_t> rootChildren;
    bool haveRoot = false;
    for (uint32_t block = 0; block < header.numBlocks; ++block) {
        const std::string& type = BlockType(header, block);
        if (type != "NiNode" && type != "BSFadeNode") continue;
        std::vector<uint32_t> children;
        NifTransform transform;
        if (ParseNode(BlockData(nif, header, block), header.blockSizes[block],
                      header, transform, children)) {
            rootTransform = transform;
            rootChildren = std::move(children);
            haveRoot = true;
            break;
        }
    }

    for (uint32_t block = 0; block < header.numBlocks; ++block) {
        const std::string& type = BlockType(header, block);
        if (type != "NiTriStrips" && type != "NiTriShape") continue;

        ShapeObject shape;
        shape.block = block;
        if (!ParseShapeObject(BlockData(nif, header, block), header.blockSizes[block],
                              header, shape)) continue;

        const bool directRootChild = haveRoot &&
            std::find(rootChildren.begin(), rootChildren.end(), block) != rootChildren.end();
        Fo3StaticNifMesh candidate;
        if (!TryLoadShape(nif, header, shape,
                          directRootChild ? &rootTransform : nullptr,
                          candidate)) continue;

        candidate.modelPath = resolved;
        Q6C_LOGI("Q6C NIF SHAPE READY: model=%s shape=%u type=%s data=%u vertices=%zu triangles=%zu diffuse=%s normal=%s rootTransform=%d",
                 resolved.c_str(), block, type.c_str(), shape.dataRef,
                 candidate.positions.size() / 3u, candidate.indices.size() / 3u,
                 candidate.diffuseTexturePath.empty() ? "<none>" : candidate.diffuseTexturePath.c_str(),
                 candidate.normalTexturePath.empty() ? "<none>" : candidate.normalTexturePath.c_str(),
                 directRootChild ? 1 : 0);
        outMeshes.push_back(std::move(candidate));
    }

    if (outMeshes.empty()) {
        Q6C_LOGW("Q6C NIF NO SUPPORTED STATIC SHAPE: %s blocks=%u", resolved.c_str(), header.numBlocks);
        return false;
    }
    Q6C_LOGI("Q6C NIF MODEL READY: model=%s visibleShapes=%zu", resolved.c_str(), outMeshes.size());
    return true;
}

bool LoadFo3StaticNif(const std::string& modelPath, Fo3StaticNifMesh& outMesh) {
    std::vector<Fo3StaticNifMesh> meshes;
    if (!LoadFo3StaticNifMeshes(modelPath, meshes) || meshes.empty()) return false;
    outMesh = std::move(meshes.front());
    return true;
}
]=])
string(REPLACE "${Q6C_OLD_LOADER}" "${Q6C_NEW_LOADER}"
       Q6C_NIF_SOURCE "${Q6C_NIF_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6c.cpp" "${Q6C_NIF_SOURCE}")

add_library(falloutquest SHARED
    ${CMAKE_CURRENT_BINARY_DIR}/q6c-native-generated.cpp
    fo3-megaton-scene.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6c.cpp
    fo3-nif-collision.cpp
    fo3-collision-overlay.cpp
    fo3-bsa-reader.cpp
    fo3-texture-bsa.cpp
    fo3-data-probe.cpp
    fo3-esm-scan.cpp
    fo3-megaton-contents.cpp
    fo3-bsa-extract.cpp
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

target_include_directories(falloutquest PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(falloutquest PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/openxr-platform-prelude.h
)

find_library(android-lib android)
find_library(log-lib log)
find_library(egl-lib EGL)
find_library(gles-lib GLESv3)
find_library(z-lib z)

target_link_libraries(falloutquest
    OpenXR::openxr_loader
    ${android-lib}
    ${log-lib}
    ${egl-lib}
    ${gles-lib}
    ${z-lib}
)
