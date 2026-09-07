# Q10.0: first authored Fallout 3 exterior environment slice.
# - Resolve WRLD -> CLMT -> WTHR from Fallout3.esm.
# - Use WTHR day sky/ambient/sunlight colours in the GLES renderers.
# - Boot through the same exterior transition path, but seed it from the actual
#   Wasteland -> MegatonEntrance XTEL instead of the old player-house test scene.
# No movement/collision-controller logic is modified here.

# -----------------------------------------------------------------------------
# Exterior CELL runtime: discover Bethesda's real Megaton gate XTEL at runtime.
# -----------------------------------------------------------------------------
string(REPLACE
    "#include \"fo3-transition-q74.h\""
    "#include \"fo3-transition-q74.h\"\n#include \"fo3-environment-q1000.h\""
    Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")

set(Q1000_GATE_ENTRY [=[
bool QueueFo3MegatonEntryQ1000() {
    constexpr uint32_t MEGATON_ENTRANCE_CELL = 0x00002DBDu;
    constexpr uint32_t MEGATON_WORLDSPACE = 0x00000A74u;
    constexpr uint32_t WASTELAND_WORLDSPACE = 0x0000003Cu;

    std::unordered_set<uint32_t> entranceRefs;
    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) {
        Q71_LOGE("Q10.0 MEGATON ENTRY FAILED: reason=open-esm");
        return false;
    }
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        Q71_LOGE("Q10.0 MEGATON ENTRY FAILED: reason=bad-esm-size");
        return false;
    }

    // Pass 1: collect every REFR actually owned by MegatonEntrance. XTEL stores
    // a destination reference, so this gives us the authoritative target set
    // without hard-coding a gate reference FormID.
    std::vector<GroupFrame> groups;
    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrame{offset + sizeField,
                                        ReadLe32(header + 8u),
                                        ReadLe32(header + 12u)});
            continue;
        }

        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        bool inEntrance = false;
        for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
            if (it->label == MEGATON_ENTRANCE_CELL &&
                (it->type == 6u || it->type == 8u ||
                 it->type == 9u || it->type == 10u)) {
                inEntrance = true;
                break;
            }
        }
        if (inEntrance && std::memcmp(header, "REFR", 4u) == 0) {
            entranceRefs.insert(ReadLe32(header + 12u));
        }
        if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
    }

    if (entranceRefs.empty()) {
        std::fclose(file);
        Q71_LOGE("Q10.0 MEGATON ENTRY FAILED: cell=%08X reason=no-owned-refs",
                 MEGATON_ENTRANCE_CELL);
        return false;
    }

    // Pass 2: find an exterior-world REFR whose XTEL targets one of those refs.
    // Prefer the Capital Wasteland WRLD explicitly; a non-Megaton exterior is a
    // fallback only so this stays data-driven if ownership differs slightly.
    if (fseeko(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }
    groups.clear();

    struct EntryCandidateQ1000 {
        uint32_t sourceRef = 0u;
        uint32_t destinationRef = 0u;
        uint32_t sourceCell = 0u;
        uint32_t sourceWorld = 0u;
        uint32_t flags = 0u;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        float rx = 0.0f, ry = 0.0f, rz = 0.0f;
        int score = -1;
    } best;

    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE ||
                offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrame{offset + sizeField,
                                        ReadLe32(header + 8u),
                                        ReadLe32(header + 12u)});
            continue;
        }

        const uint32_t recordFlags = ReadLe32(header + 8u);
        const uint32_t sourceRef = ReadLe32(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        uint32_t sourceCell = 0u;
        uint32_t sourceWorld = 0u;
        for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
            if (sourceCell == 0u &&
                (it->type == 6u || it->type == 8u ||
                 it->type == 9u || it->type == 10u)) {
                sourceCell = it->label;
            }
            if (sourceWorld == 0u && it->type == 1u) sourceWorld = it->label;
        }

        // Interior doors have no worldspace. Megaton-internal doors should not
        // be mistaken for the town entrance either.
        if (sourceWorld == 0u || sourceWorld == MEGATON_WORLDSPACE) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, recordFlags, payload)) break;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "XTEL", 4u) != 0 || size < 28u) return;
            const uint32_t destinationRef = ReadLe32(bytes + 0u);
            if (entranceRefs.find(destinationRef) == entranceRefs.end()) return;
            const int score = sourceWorld == WASTELAND_WORLDSPACE ? 100 : 10;
            if (score <= best.score) return;
            best.sourceRef = sourceRef;
            best.destinationRef = destinationRef;
            best.sourceCell = sourceCell;
            best.sourceWorld = sourceWorld;
            best.x = ReadLeFloat(bytes + 4u);
            best.y = ReadLeFloat(bytes + 8u);
            best.z = ReadLeFloat(bytes + 12u);
            best.rx = ReadLeFloat(bytes + 16u);
            best.ry = ReadLeFloat(bytes + 20u);
            best.rz = ReadLeFloat(bytes + 24u);
            best.flags = size >= 32u ? ReadLe32(bytes + 28u) : 0u;
            best.score = score;
        });
    }
    std::fclose(file);

    if (best.score < 0 || best.destinationRef == 0u) {
        Q71_LOGE("Q10.0 MEGATON ENTRY FAILED: cell=%08X targetRefs=%zu reason=no-exterior-XTEL",
                 MEGATON_ENTRANCE_CELL, entranceRefs.size());
        return false;
    }

    gPendingTransitionQ74 = {};
    gPendingTransitionQ74.destinationDoorRef = best.destinationRef;
    gPendingTransitionQ74.cellFormId = MEGATON_ENTRANCE_CELL;
    gPendingTransitionQ74.worldspaceFormId = MEGATON_WORLDSPACE;
    gPendingTransitionQ74.x = best.x;
    gPendingTransitionQ74.y = best.y;
    gPendingTransitionQ74.z = best.z;
    gPendingTransitionQ74.rx = best.rx;
    gPendingTransitionQ74.ry = best.ry;
    gPendingTransitionQ74.rz = best.rz;
    gPendingTransitionQ74.valid = true;
    gHasPendingTransitionQ74 = true;

    Q71_LOGI("Q10.0 MEGATON ENTRY: sourceDoor=%08X sourceCell=%08X sourceWorld=%08X destinationDoor=%08X destinationCell=%08X destinationWorld=%08X XTEL=(%.2f %.2f %.2f) R=(%.4f %.4f %.4f) source=Fallout3.esm",
             best.sourceRef, best.sourceCell, best.sourceWorld,
             best.destinationRef, MEGATON_ENTRANCE_CELL, MEGATON_WORLDSPACE,
             best.x, best.y, best.z, best.rx, best.ry, best.rz);
    return true;
}

]=])
string(REPLACE
    "bool ConsumeFo3CellTransitionRequestQ74(Fo3CellTransitionRequestQ74& outRequest) {"
    "${Q1000_GATE_ENTRY}bool ConsumeFo3CellTransitionRequestQ74(Fo3CellTransitionRequestQ74& outRequest) {"
    Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")

# Environment follows the active exterior worldspace. This is deliberately
# adjacent to terrain activation because both are authored WRLD-level systems.
set(Q1000_OLD_COMPLETE_ENV [=[
    bool terrainReady = false;
    if (gPendingTransitionQ74.valid &&
]=])
set(Q1000_NEW_COMPLETE_ENV [=[
    if (gPendingTransitionQ74.valid && gPendingTransitionQ74.worldspaceFormId != 0u) {
        LoadFo3EnvironmentQ1000(gPendingTransitionQ74.worldspaceFormId);
    } else {
        ResetFo3EnvironmentQ1000();
    }

    bool terrainReady = false;
    if (gPendingTransitionQ74.valid &&
]=])
string(REPLACE "${Q1000_OLD_COMPLETE_ENV}" "${Q1000_NEW_COMPLETE_ENV}"
       Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")

# Rewrite Q7.20's already-generated transition source with the Q10.0 additions.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-cell-spawn-q720.cpp"
     "${Q720_CELL_SOURCE_TEXT}")

# -----------------------------------------------------------------------------
# Static/NIF renderer: replace fixed white lighting with authored WTHR colours.
# -----------------------------------------------------------------------------
string(REPLACE
    "#include \"fo3-transition-q74.h\""
    "#include \"fo3-transition-q74.h\"\n#include \"fo3-environment-q1000.h\"\n\nbool QueueFo3MegatonEntryQ1000();"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "GLint gAlphaThresholdLocation = -1;"
    "GLint gAlphaThresholdLocation = -1;\nGLint gAmbientColorLocationQ1000 = -1;\nGLint gSunlightColorLocationQ1000 = -1;\nGLint gSunDirectionLocationQ1000 = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "        uniform float uAlphaThreshold;"
    "        uniform float uAlphaThreshold;\n        uniform vec3 uAmbientColor;\n        uniform vec3 uSunlightColor;\n        uniform vec3 uSunDirection;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "            vec3 lightDirection = normalize(vec3(0.35, 0.85, 0.40));"
    "            vec3 lightDirection = normalize(uSunDirection);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "            vec3 lit = diffuseTexel.rgb * (0.34 + 0.66 * lambert) + vec3(specular);"
    "            vec3 lit = diffuseTexel.rgb * (uAmbientColor + uSunlightColor * lambert) + uSunlightColor * specular;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "    gAlphaThresholdLocation = glGetUniformLocation(gProgram, \"uAlphaThreshold\");"
    "    gAlphaThresholdLocation = glGetUniformLocation(gProgram, \"uAlphaThreshold\");\n    gAmbientColorLocationQ1000 = glGetUniformLocation(gProgram, \"uAmbientColor\");\n    gSunlightColorLocationQ1000 = glGetUniformLocation(gProgram, \"uSunlightColor\");\n    gSunDirectionLocationQ1000 = glGetUniformLocation(gProgram, \"uSunDirection\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1000_OBJECT_LIGHT_UNIFORMS [=[
    const Fo3EnvironmentQ1000& q1000Env = GetFo3EnvironmentQ1000();
    if (q1000Env.valid) {
        glUniform3fv(gAmbientColorLocationQ1000, 1, q1000Env.ambient);
        glUniform3fv(gSunlightColorLocationQ1000, 1, q1000Env.sunlight);
        glUniform3fv(gSunDirectionLocationQ1000, 1, q1000Env.sunDirection);
    } else {
        glUniform3f(gAmbientColorLocationQ1000, 0.34f, 0.34f, 0.34f);
        glUniform3f(gSunlightColorLocationQ1000, 0.66f, 0.66f, 0.66f);
        glUniform3f(gSunDirectionLocationQ1000, 0.35f, 0.85f, 0.40f);
    }
]=])
string(REPLACE
    "    glUniform1i(gNormalLocation, 1);"
    "    glUniform1i(gNormalLocation, 1);\n${Q1000_OBJECT_LIGHT_UNIFORMS}"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Startup remains on the proven scene creation path, then immediately performs
# the normal exterior swap before the first rendered frame. This avoids a new
# special-case exterior renderer while making the visible boot location the
# authored Megaton gate XTEL.
set(Q1000_OLD_GEN_FRAMEBUFFERS [=[
void Q6HGenFramebuffers(GLsizei n, GLuint* framebuffers) {
    glGenFramebuffers(n, framebuffers);
    InitializeScene();
}
]=])
set(Q1000_NEW_GEN_FRAMEBUFFERS [=[
void Q6HGenFramebuffers(GLsizei n, GLuint* framebuffers) {
    glGenFramebuffers(n, framebuffers);
    const bool bootstrapReady = InitializeScene();
    if (bootstrapReady) {
        if (QueueFo3MegatonEntryQ1000()) {
            if (!ProcessQ74TransitionRequest()) {
                Q6H_LOGE("Q10.0 BOOT ENTRY FAILED: authored gate queued but exterior swap failed");
            }
        } else {
            Q6H_LOGE("Q10.0 BOOT ENTRY FAILED: authored Megaton gate XTEL not found; bootstrap scene retained");
        }
    }
}
]=])
string(REPLACE "${Q1000_OLD_GEN_FRAMEBUFFERS}" "${Q1000_NEW_GEN_FRAMEBUFFERS}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# LAND renderer: same authored ambient/sunlight colours as static geometry.
# -----------------------------------------------------------------------------
string(REPLACE
    "#include \"fo3-terrain-q76.h\""
    "#include \"fo3-terrain-q76.h\"\n#include \"fo3-environment-q1000.h\""
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "GLint q76bUseTextureLocation = -1;"
    "GLint q76bUseTextureLocation = -1;\nGLint q1000TerrainAmbientLocation = -1;\nGLint q1000TerrainSunlightLocation = -1;\nGLint q1000TerrainSunDirectionLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        uniform int uUseTexture;"
    "        uniform int uUseTexture;\n        uniform vec3 uAmbientColor;\n        uniform vec3 uSunlightColor;\n        uniform vec3 uSunDirection;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "            vec3 lightDirection = normalize(vec3(0.35, 0.85, 0.40));"
    "            vec3 lightDirection = normalize(uSunDirection);"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "            vec3 lit = albedo * (0.48 + 0.52 * lambert);"
    "            vec3 lit = albedo * (uAmbientColor + uSunlightColor * lambert);"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q76bUseTextureLocation = -1;"
    "    q76bUseTextureLocation = -1;\n    q1000TerrainAmbientLocation = -1;\n    q1000TerrainSunlightLocation = -1;\n    q1000TerrainSunDirectionLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q76bUseTextureLocation = glGetUniformLocation(q76bProgram, \"uUseTexture\");"
    "    q76bUseTextureLocation = glGetUniformLocation(q76bProgram, \"uUseTexture\");\n    q1000TerrainAmbientLocation = glGetUniformLocation(q76bProgram, \"uAmbientColor\");\n    q1000TerrainSunlightLocation = glGetUniformLocation(q76bProgram, \"uSunlightColor\");\n    q1000TerrainSunDirectionLocation = glGetUniformLocation(q76bProgram, \"uSunDirection\");"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "if (q76bMvpLocation < 0 || q76bSamplerLocation < 0 || q76bUseTextureLocation < 0)"
    "if (q76bMvpLocation < 0 || q76bSamplerLocation < 0 || q76bUseTextureLocation < 0 || q1000TerrainAmbientLocation < 0 || q1000TerrainSunlightLocation < 0 || q1000TerrainSunDirectionLocation < 0)"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1000_TERRAIN_LIGHT_UNIFORMS [=[
    const Fo3EnvironmentQ1000& q1000Env = GetFo3EnvironmentQ1000();
    if (q1000Env.valid) {
        glUniform3fv(q1000TerrainAmbientLocation, 1, q1000Env.ambient);
        glUniform3fv(q1000TerrainSunlightLocation, 1, q1000Env.sunlight);
        glUniform3fv(q1000TerrainSunDirectionLocation, 1, q1000Env.sunDirection);
    } else {
        glUniform3f(q1000TerrainAmbientLocation, 0.48f, 0.48f, 0.48f);
        glUniform3f(q1000TerrainSunlightLocation, 0.52f, 0.52f, 0.52f);
        glUniform3f(q1000TerrainSunDirectionLocation, 0.35f, 0.85f, 0.40f);
    }
]=])
string(REPLACE
    "    glUniform1i(q76bSamplerLocation, 0);"
    "    glUniform1i(q76bSamplerLocation, 0);\n${Q1000_TERRAIN_LIGHT_UNIFORMS}"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

# -----------------------------------------------------------------------------
# OpenXR eye pass: replace the flat green clear with an authored WTHR sky dome.
# -----------------------------------------------------------------------------
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/q4-native.cpp" Q1000_Q4_SOURCE)
string(REPLACE
    "#include \"fo3-megaton-scene.h\""
    "#include \"fo3-megaton-scene.h\"\n#include \"fo3-environment-q1000.h\""
    Q1000_Q4_SOURCE "${Q1000_Q4_SOURCE}")

set(Q1000_OLD_CLEAR [=[
            glClearColor(0.018f, 0.028f, 0.020f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
]=])
set(Q1000_NEW_CLEAR [=[
            const Fo3EnvironmentQ1000& q1000Env = GetFo3EnvironmentQ1000();
            if (q1000Env.valid) {
                glClearColor(q1000Env.horizon[0], q1000Env.horizon[1], q1000Env.horizon[2], 1.0f);
            } else {
                glClearColor(0.018f, 0.028f, 0.020f, 1.0f);
            }
            glClear(GL_COLOR_BUFFER_BIT);
]=])
string(REPLACE "${Q1000_OLD_CLEAR}" "${Q1000_NEW_CLEAR}"
       Q1000_Q4_SOURCE "${Q1000_Q4_SOURCE}")

set(Q1000_OLD_CAMERA [=[
            const Mat4 camera = ViewFromPose(virtualEyePose);
            const Mat4 viewProjection = Multiply(projection, camera);
]=])
set(Q1000_NEW_CAMERA [=[
            const Mat4 camera = ViewFromPose(virtualEyePose);
            Mat4 skyView = camera;
            skyView.m[12] = 0.0f;
            skyView.m[13] = 0.0f;
            skyView.m[14] = 0.0f;
            const Mat4 skyMvp = Multiply(projection, skyView);
            RenderFo3SkyQ1000(skyMvp.m);
            const Mat4 viewProjection = Multiply(projection, camera);
]=])
string(REPLACE "${Q1000_OLD_CAMERA}" "${Q1000_NEW_CAMERA}"
       Q1000_Q4_SOURCE "${Q1000_Q4_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q1000-q4-generated.cpp" "${Q1000_Q4_SOURCE}")

string(REPLACE
    "#include \"q4-native.cpp\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/q1000-q4-generated.cpp\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# CMakeLists wrote q6h-native-generated.cpp before the Q7.20+ transform chain.
# Q10.0 is the last renderer transform, so overwrite it once with final source.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
