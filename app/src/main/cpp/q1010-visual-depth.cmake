# Q10.1: visual depth pass, intentionally layered after Q10.0.
# - LAND uses authored 33x33 VNML vertex normals.
# - WTHR fogNear/fogFar/fog colour affect statics and LAND.
# - Placed REFR -> LIGH records provide bounded nearest-light shading.
# Movement/collision sources are not modified.

# -----------------------------------------------------------------------------
# LAND/VNML decode. Keep physical grounding height-only.
# -----------------------------------------------------------------------------
set(Q1010_VNML_DECODER [=[
bool DecodeVnmlQ1010(const uint8_t* bytes, uint32_t size, std::vector<float>& normals) {
    constexpr uint32_t expected = 33u * 33u * 3u;
    if (!bytes || size < expected) return false;
    normals.assign(static_cast<size_t>(33u * 33u * 3u), 0.0f);
    const int8_t* signedBytes = reinterpret_cast<const int8_t*>(bytes);
    for (size_t i = 0u; i < 33u * 33u; ++i) {
        float x = static_cast<float>(signedBytes[i * 3u + 0u]) / 127.0f;
        float y = static_cast<float>(signedBytes[i * 3u + 1u]) / 127.0f;
        float z = static_cast<float>(signedBytes[i * 3u + 2u]) / 127.0f;
        const float length = std::sqrt(x*x + y*y + z*z);
        if (length > 1e-5f) {
            x /= length; y /= length; z /= length;
        } else {
            x = 0.0f; y = 0.0f; z = 1.0f;
        }
        normals[i * 3u + 0u] = x;
        normals[i * 3u + 1u] = y;
        normals[i * 3u + 2u] = z;
    }
    return true;
}

]=])
string(REPLACE
    "bool ReadParentWorldspaceQ79(uint32_t worldspaceFormId, ParentWorldspaceQ79& out) {"
    "${Q1010_VNML_DECODER}bool ReadParentWorldspaceQ79(uint32_t worldspaceFormId, ParentWorldspaceQ79& out) {"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
string(REPLACE
    "        bool haveVhgt = false;"
    "        bool haveVhgt = false;\n        bool haveVnml = false;"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
set(Q1010_OLD_LAND_SUBRECORDS [=[
            if (std::memcmp(type, "VHGT", 4u) == 0 && !haveVhgt) {
                haveVhgt = DecodeVhgtQ76(bytes, subSize, terrain.heights);
            }
]=])
set(Q1010_NEW_LAND_SUBRECORDS [=[
            if (std::memcmp(type, "VHGT", 4u) == 0 && !haveVhgt) {
                haveVhgt = DecodeVhgtQ76(bytes, subSize, terrain.heights);
            } else if (std::memcmp(type, "VNML", 4u) == 0 && !haveVnml) {
                haveVnml = DecodeVnmlQ1010(bytes, subSize, terrain.normals);
            }
]=])
string(REPLACE "${Q1010_OLD_LAND_SUBRECORDS}" "${Q1010_NEW_LAND_SUBRECORDS}"
       Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
string(REPLACE
    "                 terrain.heights.size(),"
    "                 terrain.heights.size(),"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
string(REPLACE
    "        localCell.heights = std::move(parentCell.heights);"
    "        localCell.heights = std::move(parentCell.heights);\n        if (!parentCell.normals.empty()) localCell.normals = std::move(parentCell.normals);"
    Q720_TERRAIN_DATA_SOURCE "${Q720_TERRAIN_DATA_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-data-q720.cpp"
     "${Q720_TERRAIN_DATA_SOURCE}")

# -----------------------------------------------------------------------------
# LAND renderer: feed VNML instead of a per-triangle flat normal when present.
# -----------------------------------------------------------------------------
string(REPLACE
    "                      float alphaC = 1.0f) {"
    "                      float alphaC = 1.0f,\n                      Q76BV3 authoredA = {},\n                      Q76BV3 authoredB = {},\n                      Q76BV3 authoredC = {}) {"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    const Q76BV3 points[3]{a, b, c};"
    "    const Q76BV3 points[3]{a, b, c};\n    const Q76BV3 authoredNormals[3]{authoredA, authoredB, authoredC};"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
set(Q1010_OLD_PUSH_VERTEX [=[
        const Q76BV3& p = points[i];
        out.insert(out.end(), {
            p.x, p.y, p.z,
            n.x, n.y, n.z,
]=])
set(Q1010_NEW_PUSH_VERTEX [=[
        const Q76BV3& p = points[i];
        Q76BV3 vertexNormal = authoredNormals[i];
        const float authoredLength = std::sqrt(vertexNormal.x*vertexNormal.x +
                                               vertexNormal.y*vertexNormal.y +
                                               vertexNormal.z*vertexNormal.z);
        if (authoredLength < 1e-5f) vertexNormal = n;
        else vertexNormal = Q76BNormalize(vertexNormal);
        out.insert(out.end(), {
            p.x, p.y, p.z,
            vertexNormal.x, vertexNormal.y, vertexNormal.z,
]=])
string(REPLACE "${Q1010_OLD_PUSH_VERTEX}" "${Q1010_NEW_PUSH_VERTEX}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
set(Q1010_POINT_MARKER [=[
        auto point = [&](int x, int y) -> Q76BV3 {
            const float gameX = originX + static_cast<float>(x) * Q76B_VERTEX_SPACING;
            const float gameY = originY + static_cast<float>(y) * Q76B_VERTEX_SPACING;
            return Q76BToVr(gameX, gameY, gameHeight(x, y),
                            arrivalX, arrivalY, arrivalZ,
                            sceneForward, floorY, unitsPerMetre);
        };
]=])
set(Q1010_POINT_NORMALS [=[
        auto point = [&](int x, int y) -> Q76BV3 {
            const float gameX = originX + static_cast<float>(x) * Q76B_VERTEX_SPACING;
            const float gameY = originY + static_cast<float>(y) * Q76B_VERTEX_SPACING;
            return Q76BToVr(gameX, gameY, gameHeight(x, y),
                            arrivalX, arrivalY, arrivalZ,
                            sceneForward, floorY, unitsPerMetre);
        };
        const bool hasAuthoredNormals = cell.normals.size() == Q76B_HEIGHT_COUNT * 3u;
        auto authoredNormal = [&](int x, int y) -> Q76BV3 {
            if (!hasAuthoredNormals) return {};
            const size_t index = static_cast<size_t>(y * Q76B_LAND_SIDE + x) * 3u;
            // Fallout LAND uses game X/Y/Z with Z up; renderer uses X/Y/-Z world axes.
            return Q76BNormalize({cell.normals[index + 0u],
                                  cell.normals[index + 2u],
                                 -cell.normals[index + 1u]});
        };
]=])
string(REPLACE "${Q1010_POINT_MARKER}" "${Q1010_POINT_NORMALS}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "Q718PushTriangle(batch.vertices, p00, u0, v0, p10, u1, v0, p11, u1, v1);"
    "Q718PushTriangle(batch.vertices, p00, u0, v0, p10, u1, v0, p11, u1, v1, 1.0f, 1.0f, 1.0f, authoredNormal(x, y), authoredNormal(x + 1, y), authoredNormal(x + 1, y + 1));"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "Q718PushTriangle(batch.vertices, p00, u0, v0, p11, u1, v1, p01, u0, v1);"
    "Q718PushTriangle(batch.vertices, p00, u0, v0, p11, u1, v1, p01, u0, v1, 1.0f, 1.0f, 1.0f, authoredNormal(x, y), authoredNormal(x + 1, y + 1), authoredNormal(x, y + 1));"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
set(Q1010_OLD_ALPHA_TRI_A [=[
                    Q718PushTriangle(batch.vertices,
                                     p00, u0, v0, p10, u1, v0, p11, u1, v1,
                                     a00, a10, a11);
]=])
set(Q1010_NEW_ALPHA_TRI_A [=[
                    Q718PushTriangle(batch.vertices,
                                     p00, u0, v0, p10, u1, v0, p11, u1, v1,
                                     a00, a10, a11,
                                     authoredNormal(x, y), authoredNormal(x + 1, y),
                                     authoredNormal(x + 1, y + 1));
]=])
string(REPLACE "${Q1010_OLD_ALPHA_TRI_A}" "${Q1010_NEW_ALPHA_TRI_A}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
set(Q1010_OLD_ALPHA_TRI_B [=[
                    Q718PushTriangle(batch.vertices,
                                     p00, u0, v0, p11, u1, v1, p01, u0, v1,
                                     a00, a11, a01);
]=])
set(Q1010_NEW_ALPHA_TRI_B [=[
                    Q718PushTriangle(batch.vertices,
                                     p00, u0, v0, p11, u1, v1, p01, u0, v1,
                                     a00, a11, a01,
                                     authoredNormal(x, y), authoredNormal(x + 1, y + 1),
                                     authoredNormal(x, y + 1));
]=])
string(REPLACE "${Q1010_OLD_ALPHA_TRI_B}" "${Q1010_NEW_ALPHA_TRI_B}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# -----------------------------------------------------------------------------
# Shared local lights + fog: static/NIF renderer.
# -----------------------------------------------------------------------------
string(REPLACE
    "#include \"fo3-environment-q1000.h\""
    "#include \"fo3-environment-q1000.h\"\n#include \"fo3-visual-depth-q1010.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "        out vec2 vUv;"
    "        out vec2 vUv;\n        out vec3 vPosition;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "            vUv = aUv;"
    "            vUv = aUv;\n            vPosition = aPosition;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "        in vec2 vUv;"
    "        in vec2 vUv;\n        in vec3 vPosition;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "        uniform vec3 uSunDirection;"
    "        uniform vec3 uSunDirection;\n        uniform vec3 uEyePosition;\n        uniform vec3 uFogColor;\n        uniform float uFogNear;\n        uniform float uFogFar;\n        uniform int uLocalLightCount;\n        uniform vec4 uLocalLightPosRadius[8];\n        uniform vec4 uLocalLightColorFalloff[8];"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
set(Q1010_OLD_STATIC_LIT [=[
            vec3 lit = diffuseTexel.rgb * (uAmbientColor + uSunlightColor * lambert) + uSunlightColor * specular;
            fragColor = vec4(lit, alpha);
]=])
set(Q1010_NEW_STATIC_LIT [=[
            vec3 lit = diffuseTexel.rgb * (uAmbientColor + uSunlightColor * lambert) + uSunlightColor * specular;
            for (int i = 0; i < 8; ++i) {
                if (i >= uLocalLightCount) break;
                vec3 toLight = uLocalLightPosRadius[i].xyz - vPosition;
                float distanceToLight = length(toLight);
                float radius = max(uLocalLightPosRadius[i].w, 0.001);
                if (distanceToLight >= radius) continue;
                vec3 L = toLight / max(distanceToLight, 0.001);
                float localLambert = max(dot(mappedNormal, L), 0.0);
                float edge = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
                float attenuation = pow(edge, max(uLocalLightColorFalloff[i].w, 0.25));
                vec3 localColor = uLocalLightColorFalloff[i].rgb;
                lit += diffuseTexel.rgb * localColor * localLambert * attenuation;
            }
            float fogDistance = length(vPosition - uEyePosition);
            float fogFactor = smoothstep(uFogNear, max(uFogFar, uFogNear + 0.01), fogDistance);
            lit = mix(lit, uFogColor, fogFactor);
            fragColor = vec4(max(lit, vec3(0.0)), alpha);
]=])
string(REPLACE "${Q1010_OLD_STATIC_LIT}" "${Q1010_NEW_STATIC_LIT}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "GLint gSunDirectionLocationQ1000 = -1;"
    "GLint gSunDirectionLocationQ1000 = -1;\nGLint gEyePositionLocationQ1010 = -1;\nGLint gFogColorLocationQ1010 = -1;\nGLint gFogNearLocationQ1010 = -1;\nGLint gFogFarLocationQ1010 = -1;\nGLint gLocalLightCountLocationQ1010 = -1;\nGLint gLocalLightPosRadiusLocationQ1010 = -1;\nGLint gLocalLightColorFalloffLocationQ1010 = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    gSunDirectionLocationQ1000 = glGetUniformLocation(gProgram, \"uSunDirection\");"
    "    gSunDirectionLocationQ1000 = glGetUniformLocation(gProgram, \"uSunDirection\");\n    gEyePositionLocationQ1010 = glGetUniformLocation(gProgram, \"uEyePosition\");\n    gFogColorLocationQ1010 = glGetUniformLocation(gProgram, \"uFogColor\");\n    gFogNearLocationQ1010 = glGetUniformLocation(gProgram, \"uFogNear\");\n    gFogFarLocationQ1010 = glGetUniformLocation(gProgram, \"uFogFar\");\n    gLocalLightCountLocationQ1010 = glGetUniformLocation(gProgram, \"uLocalLightCount\");\n    gLocalLightPosRadiusLocationQ1010 = glGetUniformLocation(gProgram, \"uLocalLightPosRadius[0]\");\n    gLocalLightColorFalloffLocationQ1010 = glGetUniformLocation(gProgram, \"uLocalLightColorFalloff[0]\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
set(Q1010_STATIC_UNIFORMS [=[
    glUniform3fv(gEyePositionLocationQ1010, 1, gFo3EyePositionQ1010);
    glUniform1i(gLocalLightCountLocationQ1010, gFo3SelectedLightCountQ1010);
    glUniform4fv(gLocalLightPosRadiusLocationQ1010, FO3_SHADER_LIGHTS_Q1010,
                 gFo3SelectedLightPosRadiusQ1010);
    glUniform4fv(gLocalLightColorFalloffLocationQ1010, FO3_SHADER_LIGHTS_Q1010,
                 gFo3SelectedLightColorFalloffQ1010);
    if (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f) {
        glUniform3fv(gFogColorLocationQ1010, 1, q1000Env.fog);
        glUniform1f(gFogNearLocationQ1010, std::max(0.0f, q1000Env.fogNear / FO3_UNITS_PER_METRE));
        glUniform1f(gFogFarLocationQ1010, std::max(0.1f, q1000Env.fogFar / FO3_UNITS_PER_METRE));
    } else {
        glUniform3f(gFogColorLocationQ1010, 0.0f, 0.0f, 0.0f);
        glUniform1f(gFogNearLocationQ1010, 10000.0f);
        glUniform1f(gFogFarLocationQ1010, 10001.0f);
    }

]=])
string(REPLACE
    "    // Opaque and alpha-tested cutouts first. Alpha testing still writes depth,"
    "${Q1010_STATIC_UNIFORMS}    // Opaque and alpha-tested cutouts first. Alpha testing still writes depth,"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Shared local lights + fog: LAND renderer.
# -----------------------------------------------------------------------------
string(REPLACE
    "#include \"fo3-environment-q1000.h\""
    "#include \"fo3-environment-q1000.h\"\n#include \"fo3-visual-depth-q1010.h\""
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        out float vAlpha;"
    "        out float vAlpha;\n        out vec3 vPosition;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "            vAlpha = aAlpha;"
    "            vAlpha = aAlpha;\n            vPosition = aPosition;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        in float vAlpha;"
    "        in float vAlpha;\n        in vec3 vPosition;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        uniform vec3 uSunDirection;"
    "        uniform vec3 uSunDirection;\n        uniform vec3 uEyePosition;\n        uniform vec3 uFogColor;\n        uniform float uFogNear;\n        uniform float uFogFar;\n        uniform int uLocalLightCount;\n        uniform vec4 uLocalLightPosRadius[8];\n        uniform vec4 uLocalLightColorFalloff[8];"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
set(Q1010_OLD_TERRAIN_LIT [=[
            vec3 lit = albedo * (uAmbientColor + uSunlightColor * lambert);
            fragColor = vec4(lit, clamp(vAlpha, 0.0, 1.0));
]=])
set(Q1010_NEW_TERRAIN_LIT [=[
            vec3 lit = albedo * (uAmbientColor + uSunlightColor * lambert);
            for (int i = 0; i < 8; ++i) {
                if (i >= uLocalLightCount) break;
                vec3 toLight = uLocalLightPosRadius[i].xyz - vPosition;
                float distanceToLight = length(toLight);
                float radius = max(uLocalLightPosRadius[i].w, 0.001);
                if (distanceToLight >= radius) continue;
                vec3 L = toLight / max(distanceToLight, 0.001);
                float localLambert = max(dot(N, L), 0.0);
                float edge = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
                float attenuation = pow(edge, max(uLocalLightColorFalloff[i].w, 0.25));
                lit += albedo * uLocalLightColorFalloff[i].rgb * localLambert * attenuation;
            }
            float fogDistance = length(vPosition - uEyePosition);
            float fogFactor = smoothstep(uFogNear, max(uFogFar, uFogNear + 0.01), fogDistance);
            lit = mix(lit, uFogColor, fogFactor);
            fragColor = vec4(max(lit, vec3(0.0)), clamp(vAlpha, 0.0, 1.0));
]=])
string(REPLACE "${Q1010_OLD_TERRAIN_LIT}" "${Q1010_NEW_TERRAIN_LIT}"
       Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "GLint q1000TerrainSunDirectionLocation = -1;"
    "GLint q1000TerrainSunDirectionLocation = -1;\nGLint q1010TerrainEyeLocation = -1;\nGLint q1010TerrainFogColorLocation = -1;\nGLint q1010TerrainFogNearLocation = -1;\nGLint q1010TerrainFogFarLocation = -1;\nGLint q1010TerrainLightCountLocation = -1;\nGLint q1010TerrainLightPosRadiusLocation = -1;\nGLint q1010TerrainLightColorFalloffLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q1000TerrainSunDirectionLocation = -1;"
    "    q1000TerrainSunDirectionLocation = -1;\n    q1010TerrainEyeLocation = -1;\n    q1010TerrainFogColorLocation = -1;\n    q1010TerrainFogNearLocation = -1;\n    q1010TerrainFogFarLocation = -1;\n    q1010TerrainLightCountLocation = -1;\n    q1010TerrainLightPosRadiusLocation = -1;\n    q1010TerrainLightColorFalloffLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q1000TerrainSunDirectionLocation = glGetUniformLocation(q76bProgram, \"uSunDirection\");"
    "    q1000TerrainSunDirectionLocation = glGetUniformLocation(q76bProgram, \"uSunDirection\");\n    q1010TerrainEyeLocation = glGetUniformLocation(q76bProgram, \"uEyePosition\");\n    q1010TerrainFogColorLocation = glGetUniformLocation(q76bProgram, \"uFogColor\");\n    q1010TerrainFogNearLocation = glGetUniformLocation(q76bProgram, \"uFogNear\");\n    q1010TerrainFogFarLocation = glGetUniformLocation(q76bProgram, \"uFogFar\");\n    q1010TerrainLightCountLocation = glGetUniformLocation(q76bProgram, \"uLocalLightCount\");\n    q1010TerrainLightPosRadiusLocation = glGetUniformLocation(q76bProgram, \"uLocalLightPosRadius[0]\");\n    q1010TerrainLightColorFalloffLocation = glGetUniformLocation(q76bProgram, \"uLocalLightColorFalloff[0]\");"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
set(Q1010_TERRAIN_UNIFORMS [=[
    glUniform3fv(q1010TerrainEyeLocation, 1, gFo3EyePositionQ1010);
    glUniform1i(q1010TerrainLightCountLocation, gFo3SelectedLightCountQ1010);
    glUniform4fv(q1010TerrainLightPosRadiusLocation, FO3_SHADER_LIGHTS_Q1010,
                 gFo3SelectedLightPosRadiusQ1010);
    glUniform4fv(q1010TerrainLightColorFalloffLocation, FO3_SHADER_LIGHTS_Q1010,
                 gFo3SelectedLightColorFalloffQ1010);
    if (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f) {
        glUniform3fv(q1010TerrainFogColorLocation, 1, q1000Env.fog);
        glUniform1f(q1010TerrainFogNearLocation, std::max(0.0f, q1000Env.fogNear / 70.0f));
        glUniform1f(q1010TerrainFogFarLocation, std::max(0.1f, q1000Env.fogFar / 70.0f));
    } else {
        glUniform3f(q1010TerrainFogColorLocation, 0.0f, 0.0f, 0.0f);
        glUniform1f(q1010TerrainFogNearLocation, 10000.0f);
        glUniform1f(q1010TerrainFogFarLocation, 10001.0f);
    }

]=])
string(REPLACE
    "    for (const Q711TerrainBatch& batch : q711TerrainBatches) {\n        if (batch.alphaLayer || !batch.vao || batch.vertexCount <= 0) continue;"
    "${Q1010_TERRAIN_UNIFORMS}    for (const Q711TerrainBatch& batch : q711TerrainBatches) {\n        if (batch.alphaLayer || !batch.vao || batch.vertexCount <= 0) continue;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

# -----------------------------------------------------------------------------
# Exterior activation loads LIGH placements next to WRLD environment/terrain.
# -----------------------------------------------------------------------------
string(REPLACE
    "#include \"fo3-environment-q1000.h\""
    "#include \"fo3-environment-q1000.h\"\n#include \"fo3-visual-depth-q1010.h\""
    Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")
set(Q1010_OLD_ENV_LOAD [=[
    if (gPendingTransitionQ74.valid && gPendingTransitionQ74.worldspaceFormId != 0u) {
        LoadFo3EnvironmentQ1000(gPendingTransitionQ74.worldspaceFormId);
    } else {
        ResetFo3EnvironmentQ1000();
    }
]=])
set(Q1010_NEW_ENV_LOAD [=[
    if (gPendingTransitionQ74.valid && gPendingTransitionQ74.worldspaceFormId != 0u) {
        LoadFo3EnvironmentQ1000(gPendingTransitionQ74.worldspaceFormId);
        LoadFo3PlacedLightsQ1010(gPendingTransitionQ74.worldspaceFormId,
                                 gPendingTransitionQ74.x,
                                 gPendingTransitionQ74.y,
                                 gPendingTransitionQ74.z);
    } else {
        ResetFo3EnvironmentQ1000();
        ResetFo3PlacedLightsQ1010();
    }
]=])
string(REPLACE "${Q1010_OLD_ENV_LOAD}" "${Q1010_NEW_ENV_LOAD}"
       Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-cell-spawn-q720.cpp"
     "${Q720_CELL_SOURCE_TEXT}")

# -----------------------------------------------------------------------------
# Eye position selects the eight most relevant authored lights every eye/frame.
# -----------------------------------------------------------------------------
string(REPLACE
    "#include \"fo3-environment-q1000.h\""
    "#include \"fo3-environment-q1000.h\"\n#include \"fo3-visual-depth-q1010.h\""
    Q1000_Q4_SOURCE "${Q1000_Q4_SOURCE}")
string(REPLACE
    "            const XrPosef virtualEyePose = ToVirtualPose(view.pose);"
    "            const XrPosef virtualEyePose = ToVirtualPose(view.pose);\n            UpdateFo3VisualEyeQ1010(virtualEyePose.position.x, virtualEyePose.position.y, virtualEyePose.position.z);"
    Q1000_Q4_SOURCE "${Q1000_Q4_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q1000-q4-generated.cpp" "${Q1000_Q4_SOURCE}")

# Final generated renderer after Q10.1's shader transforms.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
