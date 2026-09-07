cmake_minimum_required(VERSION 3.22.1)
project(falloutquest LANGUAGES C CXX)

find_package(OpenXR REQUIRED CONFIG)

# Q6H keeps the proven native OpenXR/render path, but stops using the bed/radius
# proof-room selector. The CELL itself is now authoritative: every model-bearing
# REFR in MegatonPlayerHouse is attempted, with per-model CPU NIF caching.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/q6a-native.cpp" Q6H_NATIVE_SOURCE)
string(REPLACE "Q6A" "Q6H" Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "constexpr float SCENE_FORWARD = -3.20f;"
    "constexpr float SCENE_FORWARD = 0.00f;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "constexpr size_t MAX_SCENE_OBJECTS = 72u;"
    "constexpr size_t MAX_SCENE_OBJECTS = 1200u;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "constexpr size_t MAX_MODEL_ATTEMPTS = 800u;"
    "constexpr size_t MAX_MODEL_ATTEMPTS = 2000u;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
# Q7.16 removes the old proof-room 3000-unit static-shape ceiling. Large
# authored exterior structures are valid Fallout geometry and must not vanish.
string(REPLACE
    "constexpr float MAX_MODEL_EXTENT_UNITS = 3000.0f;"
    "constexpr float MAX_MODEL_EXTENT_UNITS = 20000.0f;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q6H_OLD_SORT [=[
    std::stable_sort(placements.begin(), placements.end(),
                     [&](const Fo3WorldPlacement& a, const Fo3WorldPlacement& b) {
        const float adx = a.x - centroidX, ady = a.y - centroidY, adz = a.z - centroidZ;
        const float bdx = b.x - centroidX, bdy = b.y - centroidY, bdz = b.z - centroidZ;
        return adx * adx + ady * ady + adz * adz < bdx * bdx + bdy * bdy + bdz * bdz;
    });
]=])
set(Q6H_NEW_SORT [=[
    Q6H_LOGI("Q6H CELL SOURCE: cell=000151E3 modelPlacements=%zu centroid=(%.1f %.1f %.1f) fullCell=1",
             placements.size(), centroidX, centroidY, centroidZ);
]=])
string(REPLACE "${Q6H_OLD_SORT}" "${Q6H_NEW_SORT}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q6H_OLD_BUILD [=[
bool BuildCpuObject(const Fo3WorldPlacement& placement, CpuObject& out) {
    Fo3StaticNifMesh mesh;
    if (!LoadFo3StaticNif(placement.modelPath, mesh)) return false;

    const float extent = MaxModelExtent(mesh) * placement.scale;
    if (!(extent >= 1.0f && extent <= MAX_MODEL_EXTENT_UNITS)) {
        Q6H_LOGW("Q6H SKIP SIZE: ref=%08X model=%s extent=%.1f",
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

    Q6H_LOGI("Q6H CPU OBJECT: ref=%08X base=%08X EDID=%s model=%s vertices=%zu triangles=%zu ESM=(%.1f %.1f %.1f / %.3f %.3f %.3f / %.3f)",
             placement.refFormId, placement.baseFormId,
             placement.editorId.empty() ? "<none>" : placement.editorId.c_str(),
             placement.modelPath.c_str(), vertexCount, out.mesh.indices.size() / 3u,
             placement.x, placement.y, placement.z,
             placement.rx, placement.ry, placement.rz, placement.scale);
    return true;
}
]=])
set(Q6H_NEW_BUILD [=[
bool BuildCpuObjects(const Fo3WorldPlacement& placement, std::vector<CpuObject>& outs) {
    outs.clear();
    static std::unordered_map<std::string, std::vector<Fo3StaticNifMesh>> modelCache;

    auto cached = modelCache.find(placement.modelPath);
    if (cached == modelCache.end()) {
        std::vector<Fo3StaticNifMesh> meshes;
        if (!LoadFo3StaticNifMeshes(placement.modelPath, meshes)) return false;
        cached = modelCache.emplace(placement.modelPath, std::move(meshes)).first;
        Q6H_LOGI("Q6H MODEL CACHE MISS: model=%s shapes=%zu uniqueModels=%zu",
                 placement.modelPath.c_str(), cached->second.size(), modelCache.size());
    }

    size_t shapeIndex = 0;
    for (const Fo3StaticNifMesh& cachedMesh : cached->second) {
        Fo3StaticNifMesh mesh = cachedMesh;
        const float extent = MaxModelExtent(mesh) * placement.scale;
        if (!(extent >= 1.0f && extent <= MAX_MODEL_EXTENT_UNITS)) {
            Q6H_LOGW("Q6H SKIP SHAPE SIZE: ref=%08X model=%s shape=%zu extent=%.1f",
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

        outs.push_back(std::move(out));
        ++shapeIndex;
    }
    return !outs.empty();
}
]=])
string(REPLACE "${Q6H_OLD_BUILD}" "${Q6H_NEW_BUILD}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q6H_OLD_SELECT [=[
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
set(Q6H_NEW_SELECT [=[
    std::vector<CpuObject> selected;
    selected.reserve(placements.size() * 2u);
    size_t attempts = 0;
    size_t selectedPlacements = 0;
    size_t filteredMarkersEffects = 0;
    size_t unsupportedPlacements = 0;
    std::unordered_map<std::string, size_t> attemptedTypes;
    std::unordered_map<std::string, size_t> renderedTypes;

    for (const Fo3WorldPlacement& placement : placements) {
        ++attempts;
        const std::string type = placement.baseRecordType.empty() ? "<none>" : placement.baseRecordType;
        ++attemptedTypes[type];

        if (placement.editorId == "XMarker" ||
            placement.modelPath.rfind("Effects\\", 0) == 0 ||
            placement.modelPath.rfind("effects\\", 0) == 0) {
            ++filteredMarkersEffects;
            continue;
        }

        std::vector<CpuObject> parts;
        if (BuildCpuObjects(placement, parts)) {
            ++selectedPlacements;
            ++renderedTypes[type];
            for (CpuObject& part : parts) selected.push_back(std::move(part));
        } else {
            ++unsupportedPlacements;
        }
    }

    Q6H_LOGI("Q6H CELL SELECT: esmModelPlacements=%zu attempted=%zu renderedPlacements=%zu drawShapes=%zu filteredMarkersEffects=%zu unsupported=%zu fullCell=1",
             placements.size(), attempts, selectedPlacements, selected.size(),
             filteredMarkersEffects, unsupportedPlacements);
    for (const auto& entry : attemptedTypes) {
        const auto rendered = renderedTypes.find(entry.first);
        const size_t renderedCount = rendered == renderedTypes.end() ? 0u : rendered->second;
        Q6H_LOGI("Q6H CELL TYPE: type=%s attempted=%zu rendered=%zu",
                 entry.first.c_str(), entry.second, renderedCount);
    }
]=])
string(REPLACE "${Q6H_OLD_SELECT}" "${Q6H_NEW_SELECT}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q6H_OLD_CENTER [=[
    const float centerX = (minimum.x + maximum.x) * 0.5f;
    const float centerY = (minimum.y + maximum.y) * 0.5f;
    const float floorZ = minimum.z;
]=])
set(Q6H_NEW_CENTER [=[
    float structureMinX = 1e30f, structureMinY = 1e30f;
    float structureMaxX = -1e30f, structureMaxY = -1e30f;
    size_t structureShapes = 0;
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
        ++structureShapes;
    }

    Fo3CellArrival q6kArrival;
    const bool q6kArrivalReady = LoadMegatonPlayerHouseArrival(q6kArrival) && q6kArrival.valid;
    const float fallbackCenterX = structureShapes > 0u
        ? (structureMinX + structureMaxX) * 0.5f
        : (minimum.x + maximum.x) * 0.5f;
    const float fallbackCenterY = structureShapes > 0u
        ? (structureMinY + structureMaxY) * 0.5f
        : (minimum.y + maximum.y) * 0.5f;

    const float centerX = q6kArrivalReady ? q6kArrival.x : fallbackCenterX;
    const float centerY = q6kArrivalReady ? q6kArrival.y : fallbackCenterY;
    const float floorZ = q6kArrivalReady ? q6kArrival.z : minimum.z;
    Q6H_LOGI("Q6H SPAWN SEED: source=%s center=(%.2f %.2f) floorReferenceZ=%.2f structureShapes=%zu VRseed=(0,0) collisionValidation=pending",
             q6kArrivalReady ? "Fallout3.esm/XTEL" : "structure-fallback",
             centerX, centerY, floorZ, structureShapes);
]=])
string(REPLACE "${Q6H_OLD_CENTER}" "${Q6H_NEW_CENTER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "GLuint CompileShader(GLenum type, const char* source)"
    "GLuint CompileQ6HShader(GLenum type, const char* source)"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSource);"
    "GLuint vs = CompileQ6HShader(GL_VERTEX_SHADER, vertexSource);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);"
    "GLuint fs = CompileQ6HShader(GL_FRAGMENT_SHADER, fragmentSource);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "GLuint CreateProgram()"
    "GLuint CreateQ6HProgram()"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "gProgram = CreateProgram();"
    "gProgram = CreateQ6HProgram();"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "#include \"fo3-texture-bsa.h\""
    "#include \"fo3-texture-bsa.h\"\n#include \"fo3-collision-overlay.h\"\n#include \"fo3-transition-q74.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q6H_OLD_PROGRAM [=[
    gProgram = CreateQ6HProgram();
]=])
set(Q6H_NEW_PROGRAM [=[
    gSceneCenterX = centerX;
    gSceneCenterY = centerY;
    gSceneFloorZ = floorZ;
    std::vector<Fo3WorldPlacement> q6hCollisionPlacements;
    q6hCollisionPlacements.reserve(selected.size());
    for (const CpuObject& object : selected) {
        q6hCollisionPlacements.push_back(object.placement);
    }
    const bool collisionReady = InitializeFo3CollisionOverlay(q6hCollisionPlacements,
                                                               centerX, centerY, floorZ,
                                                               SCENE_FORWARD, FLOOR_Y,
                                                               FO3_UNITS_PER_METRE);
    Q6H_LOGI("Q6H COLLISION WORLD: requestedRefs=%zu ready=%d safeSpawnValidation=firstFrame",
             q6hCollisionPlacements.size(), collisionReady ? 1 : 0);

    gProgram = CreateQ6HProgram();
]=])
string(REPLACE "${Q6H_OLD_PROGRAM}" "${Q6H_NEW_PROGRAM}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q74_RENDER_HOOK [=[
bool Q74ShouldSkipPlacement(const Fo3WorldPlacement& placement) {
    std::string edid = placement.editorId;
    std::string model = placement.modelPath;
    for (char& ch : edid) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    for (char& ch : model) {
        if (ch == '/') ch = '\\';
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    if (placement.baseRecordType == "IDLM") return true;
    if (model.rfind("effects\\", 0) == 0) return true;
    if (edid == "xmarker" || edid == "xmarkerheading") return true;
    if (edid.find("marker") != std::string::npos) return true;
    if (edid == "counterlean" || edid == "barkeep") return true;
    return false;
}

void Q74DeleteGpuObjects(std::vector<GpuObject>& objects) {
    for (GpuObject& object : objects) {
        if (object.vbo) glDeleteBuffers(1, &object.vbo);
        if (object.vao) glDeleteVertexArrays(1, &object.vao);
        object.vbo = 0;
        object.vao = 0;
    }
    objects.clear();
}

bool ProcessQ74TransitionRequest() {
    Fo3CellTransitionRequestQ74 request;
    if (!ConsumeFo3CellTransitionRequestQ74(request) || !request.valid) return false;

    Q6H_LOGI("Q7.4 SCENE SWAP BEGIN: door=%08X cell=%08X worldspace=%08X XTEL=(%.2f %.2f %.2f)",
             request.destinationDoorRef, request.cellFormId, request.worldspaceFormId,
             request.x, request.y, request.z);

    std::vector<Fo3WorldPlacement> placements;
    if (!LoadFo3CellPlacementsQ74(request.cellFormId, placements)) {
        Q6H_LOGE("Q7.4 SCENE SWAP FAILED: cell=%08X reason=cell-loader oldSceneRetained=1",
                 request.cellFormId);
        return false;
    }

    std::vector<CpuObject> selected;
    selected.reserve(placements.size() * 2u);
    size_t skipped = 0u;
    size_t unsupported = 0u;
    for (const Fo3WorldPlacement& placement : placements) {
        if (Q74ShouldSkipPlacement(placement)) {
            ++skipped;
            continue;
        }
        std::vector<CpuObject> parts;
        if (!BuildCpuObjects(placement, parts)) {
            ++unsupported;
            continue;
        }
        for (CpuObject& part : parts) selected.push_back(std::move(part));
    }
    if (selected.size() < 2u) {
        Q6H_LOGE("Q7.4 SCENE SWAP FAILED: cell=%08X drawShapes=%zu skipped=%zu unsupported=%zu oldSceneRetained=1",
                 request.cellFormId, selected.size(), skipped, unsupported);
        return false;
    }

    std::vector<GpuObject> replacement;
    replacement.reserve(selected.size());
    for (CpuObject& cpu : selected) {
        GpuObject gpu;
        if (UploadCpuObject(cpu, request.x, request.y, request.z, gpu)) {
            replacement.push_back(std::move(gpu));
        } else {
            if (gpu.vbo) glDeleteBuffers(1, &gpu.vbo);
            if (gpu.vao) glDeleteVertexArrays(1, &gpu.vao);
        }
    }
    if (replacement.size() < 2u) {
        Q74DeleteGpuObjects(replacement);
        Q6H_LOGE("Q7.4 SCENE SWAP FAILED: cell=%08X gpuObjects=%zu oldSceneRetained=1",
                 request.cellFormId, replacement.size());
        return false;
    }

    std::vector<Fo3WorldPlacement> collisionPlacements;
    collisionPlacements.reserve(selected.size());
    for (const CpuObject& cpu : selected) collisionPlacements.push_back(cpu.placement);

    const bool collisionReady = InitializeFo3CollisionOverlay(collisionPlacements,
                                                               request.x, request.y, request.z,
                                                               SCENE_FORWARD, FLOOR_Y,
                                                               FO3_UNITS_PER_METRE);

    const size_t oldObjects = gObjects.size();
    Q74DeleteGpuObjects(gObjects);
    gObjects = std::move(replacement);
    gSceneReady = !gObjects.empty();
    gLoggedFirstDraw = false;

    size_t triangles = 0u;
    size_t realDiffuse = 0u;
    size_t realNormal = 0u;
    for (const GpuObject& object : gObjects) {
        triangles += static_cast<size_t>(object.vertexCount / 3);
        if (object.realDiffuse) ++realDiffuse;
        if (object.realNormal) ++realNormal;
    }

    CompleteFo3CellTransitionQ74(request.cellFormId);
    Q6H_LOGI("Q7.4 SCENE SWAP READY: cell=%08X worldspace=%08X oldObjects=%zu newObjects=%zu triangles=%zu realDiffuse=%zu realNormal=%zu skippedMarkersEffects=%zu unsupported=%zu collisionReady=%d XTELorigin=1 orientationPreserved=1",
             request.cellFormId, request.worldspaceFormId,
             oldObjects, gObjects.size(), triangles, realDiffuse, realNormal,
             skipped, unsupported, collisionReady ? 1 : 0);
    return true;
}

]=])
string(REPLACE
    "void DrawSceneObject(const GpuObject& object) {"
    "${Q74_RENDER_HOOK}void DrawSceneObject(const GpuObject& object) {"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "void RenderScene() {\n    if (!gSceneReady || !gProgram || gObjects.empty()) return;"
    "void RenderScene() {\n    ProcessQ74TransitionRequest();\n    if (!gSceneReady || !gProgram || gObjects.empty()) return;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q6H_OLD_RENDER_RESTORE [=[
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
]=])
set(Q6H_NEW_RENDER_RESTORE [=[
    RenderFo3CollisionOverlay(mvp);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
]=])
string(REPLACE "${Q6H_OLD_RENDER_RESTORE}" "${Q6H_NEW_RENDER_RESTORE}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q6H_OLD_CLEANUP [=[
    glDeleteFramebuffers(n, framebuffers);
    for (GpuObject& object : gObjects) {
]=])
set(Q6H_NEW_CLEANUP [=[
    glDeleteFramebuffers(n, framebuffers);
    ShutdownFo3CollisionOverlay();
    for (GpuObject& object : gObjects) {
]=])
string(REPLACE "${Q6H_OLD_CLEANUP}" "${Q6H_NEW_CLEANUP}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Q7.16 makes static-coverage diagnostics easy to isolate in one adb filter.
string(REPLACE "Q6H CELL SELECT:" "Q7.16 STATIC COVERAGE:"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE "Q6H SKIP SHAPE SIZE:" "Q7.16 STATIC SKIP SIZE:"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-static-nif.cpp" Q6H_NIF_SOURCE)
string(REPLACE "Q6A" "Q6H" Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")
set(Q6H_OLD_LOADER [=[
bool LoadFo3StaticNif(const std::string& modelPath, Fo3StaticNifMesh& outMesh) {
    outMesh = {};
    outMesh.modelPath = modelPath;

    std::vector<uint8_t> nif;
    std::string resolved;
    if (!LoadFalloutMeshFile(modelPath, nif, &resolved)) return false;
    if (nif.empty() || nif.size() > MAX_NIF_BYTES) return false;

    NifHeader header;
    if (!ParseHeader(nif, header)) {
        Q6H_LOGW("Q6H NIF UNSUPPORTED HEADER: %s", resolved.c_str());
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
        Q6H_LOGI("Q6H NIF READY: model=%s shape=%u type=%s data=%u vertices=%zu triangles=%zu diffuse=%s normal=%s rootTransform=%d",
                 resolved.c_str(), block, type.c_str(), shape.dataRef,
                 outMesh.positions.size() / 3u, outMesh.indices.size() / 3u,
                 outMesh.diffuseTexturePath.empty() ? "<none>" : outMesh.diffuseTexturePath.c_str(),
                 outMesh.normalTexturePath.empty() ? "<none>" : outMesh.normalTexturePath.c_str(),
                 directRootChild ? 1 : 0);
        return true;
    }

    Q6H_LOGW("Q6H NIF NO SUPPORTED STATIC SHAPE: %s blocks=%u", resolved.c_str(), header.numBlocks);
    return false;
}
]=])
set(Q6H_NEW_LOADER [=[
bool LoadFo3StaticNifMeshes(const std::string& modelPath,
                            std::vector<Fo3StaticNifMesh>& outMeshes) {
    outMeshes.clear();

    std::vector<uint8_t> nif;
    std::string resolved;
    if (!LoadFalloutMeshFile(modelPath, nif, &resolved)) return false;
    if (nif.empty() || nif.size() > MAX_NIF_BYTES) return false;

    NifHeader header;
    if (!ParseHeader(nif, header)) {
        Q6H_LOGW("Q6H NIF UNSUPPORTED HEADER: %s", resolved.c_str());
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
        outMeshes.push_back(std::move(candidate));
    }

    if (outMeshes.empty()) {
        Q6H_LOGW("Q6H NIF NO SUPPORTED STATIC SHAPE: %s blocks=%u", resolved.c_str(), header.numBlocks);
        return false;
    }
    Q6H_LOGI("Q6H NIF MODEL READY: model=%s visibleShapes=%zu", resolved.c_str(), outMeshes.size());
    return true;
}

bool LoadFo3StaticNif(const std::string& modelPath, Fo3StaticNifMesh& outMesh) {
    std::vector<Fo3StaticNifMesh> meshes;
    if (!LoadFo3StaticNifMeshes(modelPath, meshes) || meshes.empty()) return false;
    outMesh = std::move(meshes.front());
    return true;
}
]=])
string(REPLACE "${Q6H_OLD_LOADER}" "${Q6H_NEW_LOADER}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp" "${Q6H_NIF_SOURCE}")

# Q7.4 broadens authored collision from the player-house ShackInteriors subset
# to all Megaton architecture. It also consumes the one-shot post-swap player
# reset before normal collision grounding runs.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-collision-overlay.cpp" Q74_COLLISION_SOURCE)
string(REPLACE
    "#include \"fo3-nif-collision-q6f.h\""
    "#include \"fo3-nif-collision-q6f.h\"\n#include \"fo3-transition-q74.h\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
string(REPLACE
    "return lower.find(\"architecture\\\\megaton\\\\interior\\\\shackinteriors\") != std::string::npos;"
    "return lower.find(\"architecture\\\\megaton\\\\\") != std::string::npos;"
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
set(Q74_OLD_RESOLVE_GUARD [=[
    if (!outX || !outZ || !outPlayerYOffset || !gPlayerCollisionReady ||
        gWorldTriangles.empty()) return false;

    ++gResolveCounter;
]=])
set(Q74_NEW_RESOLVE_GUARD [=[
    if (!outX || !outZ || !outPlayerYOffset) return false;
    if (ConsumeFo3PlayerResetQ74()) {
        *outX = 0.0f;
        *outZ = 0.0f;
        *outPlayerYOffset = 0.0f;
        gSafeSpawnResolved = false;
        Q6G_LOGI("Q7.4 PLAYER RESET: bodyCenter=(0,0) source=XTEL newCollisionReady=%d",
                 gPlayerCollisionReady ? 1 : 0);
        return true;
    }
    if (!gPlayerCollisionReady || gWorldTriangles.empty()) return false;

    ++gResolveCounter;
]=])
string(REPLACE "${Q74_OLD_RESOLVE_GUARD}" "${Q74_NEW_RESOLVE_GUARD}"
       Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# Q7.18 keeps the old resolver available for interiors/safe-spawn, then injects
# the exterior sweep-slide-step-over replacement immediately before the public
# ready helpers. This avoids destabilising the already-proven interior path.
string(REPLACE
    "bool ResolveFo3PlayerMotionQ6G("
    "bool ResolveFo3PlayerMotionLegacyQ716("
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
string(REPLACE
    "bool IsFo3PlayerCollisionReadyQ6G() {"
    "#include \"fo3-collision-controller-q716.inc\"\n\nbool IsFo3PlayerCollisionReadyQ6G() {"
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
string(REPLACE
    "Q7.14 PHYSICS READY:"
    "Q7.18 PHYSICS READY: controller=sweep-slide-step-over persistentOverlap=depth-aware"
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-overlay-q74.cpp" "${Q74_COLLISION_SOURCE}")

add_library(falloutquest SHARED
    ${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp
    fo3-megaton-scene.cpp
    fo3-cell-spawn-q74.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp
    fo3-nif-collision.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-overlay-q74.cpp
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