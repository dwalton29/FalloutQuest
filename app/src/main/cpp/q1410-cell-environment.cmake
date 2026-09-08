# Q14.1: use the environment authored on Megaton exterior CELLs rather than the
# generic WRLD/CLMT weather chosen by Q10.0. This patch deliberately leaves the
# Q13.9 colour-domain experiment unchanged so the visual test isolates record
# ownership: CELL XCLR -> REGN RDWT weather, CELL XCIM -> IMGS, plus WTHR fog power.

# -----------------------------------------------------------------------------
# Generate a Q14.1 variant of the Q14.0 time-of-day header. The Q14.0 test-clock
# input remains intact; only its source weather/imagespace and fog fields change.
# -----------------------------------------------------------------------------
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-time-of-day-q1400.h" Q1410_TOD_HEADER)

string(REPLACE
    "#include \"fo3-weather-sky-q1330.h\""
    "#include \"fo3-weather-sky-q1330.h\"\n#include \"fo3-cell-environment-q1410.h\""
    Q1410_TOD_HEADER "${Q1410_TOD_HEADER}")

# Preserve both FNAM Power values alongside the already-parsed Day/Night distances.
set(Q1410_OLD_FOG_FIELDS [=[
    float fogNightNear = 0.0f;
    float fogNightFar = 0.0f;
    bool haveFog = false;
]=])
set(Q1410_NEW_FOG_FIELDS [=[
    float fogNightNear = 0.0f;
    float fogNightFar = 0.0f;
    float fogDayPower = 1.0f;
    float fogNightPower = 1.0f;
    bool haveFog = false;
]=])
string(REPLACE "${Q1410_OLD_FOG_FIELDS}" "${Q1410_NEW_FOG_FIELDS}"
       Q1410_TOD_HEADER "${Q1410_TOD_HEADER}")

set(Q1410_OLD_FNAM [=[
        if (std::memcmp(type, "FNAM", 4u) == 0 && size >= 16u) {
            out.fogDayNear = ReadFloat(bytes + 0u);
            out.fogDayFar = ReadFloat(bytes + 4u);
            out.fogNightNear = ReadFloat(bytes + 8u);
            out.fogNightFar = ReadFloat(bytes + 12u);
            out.haveFog = std::isfinite(out.fogDayNear) && std::isfinite(out.fogDayFar) &&
                          std::isfinite(out.fogNightNear) && std::isfinite(out.fogNightFar);
            return;
        }
]=])
set(Q1410_NEW_FNAM [=[
        if (std::memcmp(type, "FNAM", 4u) == 0 && size >= 24u) {
            out.fogDayNear = ReadFloat(bytes + 0u);
            out.fogDayFar = ReadFloat(bytes + 4u);
            out.fogNightNear = ReadFloat(bytes + 8u);
            out.fogNightFar = ReadFloat(bytes + 12u);
            out.fogDayPower = ReadFloat(bytes + 16u);
            out.fogNightPower = ReadFloat(bytes + 20u);
            out.haveFog = std::isfinite(out.fogDayNear) && std::isfinite(out.fogDayFar) &&
                          std::isfinite(out.fogNightNear) && std::isfinite(out.fogNightFar) &&
                          std::isfinite(out.fogDayPower) && std::isfinite(out.fogNightPower);
            return;
        }
]=])
string(REPLACE "${Q1410_OLD_FNAM}" "${Q1410_NEW_FNAM}"
       Q1410_TOD_HEADER "${Q1410_TOD_HEADER}")

# The shader consumes this once-per-frame value. It is an authored WTHR FNAM
# parameter; the pow() interpretation below is the renderer bridge.
string(REPLACE
    "inline bool gAppliedOnce = false;"
    "inline bool gAppliedOnce = false;\ninline float gFogPowerQ1410 = 1.0f;"
    Q1410_TOD_HEADER "${Q1410_TOD_HEADER}")

# Q14.1 already supplies the correct CELL XCIM image. Do not have Q14.0 reload
# it through the older Q12.9 parser and undo the corrected FO3 152-byte layout.
set(Q1410_OLD_BASE_IMAGE [=[
    const Fo3ImageSpaceQ1280 live = gFo3ImageSpaceQ1280;
    if (live.valid && live.cellFormId != 0u && live.worldspaceFormId != 0u &&
        LoadFo3ImageSpaceQ1280(live.cellFormId, live.worldspaceFormId)) {
        runtime.baseImage = gFo3ImageSpaceQ1280;
    } else {
        runtime.baseImage = live;
    }
    gFo3ImageSpaceQ1280 = live;
]=])
set(Q1410_NEW_BASE_IMAGE [=[
    const Fo3ImageSpaceQ1280 live = gFo3ImageSpaceQ1280;
    runtime.baseImage = live;
]=])
string(REPLACE "${Q1410_OLD_BASE_IMAGE}" "${Q1410_NEW_BASE_IMAGE}"
       Q1410_TOD_HEADER "${Q1410_TOD_HEADER}")

# Replace WRLD/CLMT weather ownership with the exterior CELL's region weather,
# and replace WRLD ImageSpace with the CELL's explicit XCIM before endpoint IMADs.
set(Q1410_OLD_BUILD [=[
inline bool BuildRuntime() {
    RuntimeQ1400 runtime;
    const Fo3EnvironmentQ1000& env = gFo3EnvironmentQ1000;
    if (!env.valid || env.worldspaceFormId == 0u || env.weatherFormId == 0u ||
        env.climateFormId == 0u || !gFo3ImageSpaceQ1280.valid) {
        return false;
    }

    runtime.worldspaceFormId = env.worldspaceFormId;
    runtime.climateFormId = env.climateFormId;
    runtime.weatherFormId = env.weatherFormId;
    if (!LoadClimateTimes(runtime.climateFormId, runtime.climate) ||
        !LoadWeatherTime(runtime.weatherFormId, runtime.weather)) {
        return false;
    }

    BuildEndpointImages(runtime);
]=])
set(Q1410_NEW_BUILD [=[
inline bool BuildRuntime() {
    RuntimeQ1400 runtime;
    Fo3EnvironmentQ1000& env = gFo3EnvironmentQ1000;
    if (!env.valid || env.worldspaceFormId == 0u || env.weatherFormId == 0u ||
        env.climateFormId == 0u) {
        return false;
    }

    Fo3CellEnvironmentQ1410 cellEnv;
    const uint32_t preferredCell = gFo3ImageSpaceQ1280.cellFormId;
    if (ResolveFo3CellEnvironmentQ1410(env.worldspaceFormId, preferredCell, cellEnv)) {
        env.weatherFormId = cellEnv.weatherFormId;
        env.weatherEditorId = cellEnv.weatherEditorId;
        if (!LoadFo3CellImageSpaceQ1410(cellEnv, env.worldspaceFormId)) {
            __android_log_print(ANDROID_LOG_WARN, TAG,
                                "Q14.1 CELL ENV: cell=%08X region=%08X weather=%08X XCIM=%08X result=imagespace-load-failed",
                                cellEnv.cellFormId, cellEnv.regionFormId,
                                cellEnv.weatherFormId, cellEnv.imageSpaceFormId);
        }
    }
    if (!gFo3ImageSpaceQ1280.valid) return false;

    runtime.worldspaceFormId = env.worldspaceFormId;
    runtime.climateFormId = env.climateFormId;
    runtime.weatherFormId = env.weatherFormId;
    if (!LoadClimateTimes(runtime.climateFormId, runtime.climate) ||
        !LoadWeatherTime(runtime.weatherFormId, runtime.weather)) {
        return false;
    }
    env.weatherEditorId = runtime.weather.editorId;

    BuildEndpointImages(runtime);
    if (cellEnv.valid) {
        const int fr = static_cast<int>(std::lround(runtime.weather.encoded[1][1][0] * 255.0f));
        const int fg = static_cast<int>(std::lround(runtime.weather.encoded[1][1][1] * 255.0f));
        const int fb = static_cast<int>(std::lround(runtime.weather.encoded[1][1][2] * 255.0f));
        __android_log_print(
            ANDROID_LOG_INFO, TAG,
            "Q14.1 CELL ENV READY: cell=%08X EDID=%s region=%08X EDID=%s weather=%08X EDID=%s chance=%d XCIM=%08X IMGS=%s fogDayRGB=(%d %d %d) fogDayNear=%.1f fogDayFar=%.1f fogDayPower=%.3f fogNightNear=%.1f fogNightFar=%.1f fogNightPower=%.3f source=Fallout3.esm",
            cellEnv.cellFormId,
            cellEnv.cellEditorId.empty() ? "<none>" : cellEnv.cellEditorId.c_str(),
            cellEnv.regionFormId,
            cellEnv.regionEditorId.empty() ? "<none>" : cellEnv.regionEditorId.c_str(),
            runtime.weather.formId,
            runtime.weather.editorId.empty() ? "<none>" : runtime.weather.editorId.c_str(),
            cellEnv.weatherChance, cellEnv.imageSpaceFormId,
            gFo3ImageSpaceQ1280.editorId.empty() ? "<none>" : gFo3ImageSpaceQ1280.editorId.c_str(),
            fr, fg, fb,
            runtime.weather.fogDayNear, runtime.weather.fogDayFar, runtime.weather.fogDayPower,
            runtime.weather.fogNightNear, runtime.weather.fogNightFar, runtime.weather.fogNightPower);
    }
]=])
string(REPLACE "${Q1410_OLD_BUILD}" "${Q1410_NEW_BUILD}"
       Q1410_TOD_HEADER "${Q1410_TOD_HEADER}")

# Interpolate the authored Day/Night Fog Power using the same bridge as FNAM distances.
set(Q1410_OLD_APPLY_FOG [=[
    if (runtime.weather.haveFog) {
        // FNAM has only Day and Night distances. Sunrise/Sunset receive an equal
        // day/night bridge while NAM0 still uses its dedicated four RGB endpoints.
        const float nightAmount = Clamp01(weights.w[3] + 0.5f * (weights.w[0] + weights.w[2]));
        env.fogNear = Lerp(runtime.weather.fogDayNear, runtime.weather.fogNightNear, nightAmount);
        env.fogFar = Lerp(runtime.weather.fogDayFar, runtime.weather.fogNightFar, nightAmount);
    }
]=])
set(Q1410_NEW_APPLY_FOG [=[
    if (runtime.weather.haveFog) {
        // FNAM has Day/Night Near/Far and Power values. Sunrise/Sunset receive
        // the same equal day/night bridge used by Q14.0 for fog distance.
        const float nightAmount = Clamp01(weights.w[3] + 0.5f * (weights.w[0] + weights.w[2]));
        env.fogNear = Lerp(runtime.weather.fogDayNear, runtime.weather.fogNightNear, nightAmount);
        env.fogFar = Lerp(runtime.weather.fogDayFar, runtime.weather.fogNightFar, nightAmount);
        gFogPowerQ1410 = std::max(0.001f,
            Lerp(runtime.weather.fogDayPower, runtime.weather.fogNightPower, nightAmount));
    } else {
        gFogPowerQ1410 = 1.0f;
    }
]=])
string(REPLACE "${Q1410_OLD_APPLY_FOG}" "${Q1410_NEW_APPLY_FOG}"
       Q1410_TOD_HEADER "${Q1410_TOD_HEADER}")

# Make the hourly diagnostic expose the effective fog power too.
string(REPLACE
    "fogNear=%.1f fogFar=%.1f sunDir="
    "fogNear=%.1f fogFar=%.1f fogPower=%.3f sunDir="
    Q1410_TOD_HEADER "${Q1410_TOD_HEADER}")
string(REPLACE
    "env.fog[0], env.fog[1], env.fog[2], env.fogNear, env.fogFar,\n            env.sunDirection[0]"
    "env.fog[0], env.fog[1], env.fog[2], env.fogNear, env.fogFar, gFogPowerQ1410,\n            env.sunDirection[0]"
    Q1410_TOD_HEADER "${Q1410_TOD_HEADER}")

# Guards: do not silently build if the source drifted and any key replacement failed.
string(FIND "${Q1410_TOD_HEADER}" "fo3-cell-environment-q1410.h" Q1410_HEADER_INCLUDE_OK)
string(FIND "${Q1410_TOD_HEADER}" "fogDayPower" Q1410_FOG_FIELD_OK)
string(FIND "${Q1410_TOD_HEADER}" "Q14.1 CELL ENV READY" Q1410_CELL_ENV_OK)
string(FIND "${Q1410_TOD_HEADER}" "gFogPowerQ1410" Q1410_FOG_RUNTIME_OK)
string(FIND "${Q1410_TOD_HEADER}" "runtime.baseImage = live;" Q1410_BASE_IMAGE_OK)
if(Q1410_HEADER_INCLUDE_OK EQUAL -1 OR Q1410_FOG_FIELD_OK EQUAL -1 OR
   Q1410_CELL_ENV_OK EQUAL -1 OR Q1410_FOG_RUNTIME_OK EQUAL -1 OR
   Q1410_BASE_IMAGE_OK EQUAL -1)
    message(FATAL_ERROR
        "Q14.1 time-of-day transform drifted: include=${Q1410_HEADER_INCLUDE_OK} fog=${Q1410_FOG_FIELD_OK} cell=${Q1410_CELL_ENV_OK} runtime=${Q1410_FOG_RUNTIME_OK} base=${Q1410_BASE_IMAGE_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-time-of-day-q1410.h" "${Q1410_TOD_HEADER}")

# Replace the Q14.0 header include in the final renderer with the generated Q14.1 variant.
string(REPLACE
    "#include \"fo3-time-of-day-q1400.h\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-time-of-day-q1410.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(FIND "${Q6H_NATIVE_SOURCE}" "fo3-time-of-day-q1410.h" Q1410_NATIVE_TOD_OK)
if(Q1410_NATIVE_TOD_OK EQUAL -1)
    message(FATAL_ERROR "Q14.1 could not replace Q14.0 time-of-day include")
endif()

# -----------------------------------------------------------------------------
# WTHR FNAM Fog Power in static/NIF and LAND shaders.
# Q10.1 used smoothstep(Near,Far); Q14.1 uses normalized-distance^Power so the
# authored Power value affects how quickly the weather haze fills the range.
# -----------------------------------------------------------------------------
foreach(Q1410_SHADER_VAR Q6H_NATIVE_SOURCE Q720_TERRAIN_RENDER_SOURCE)
    string(REPLACE
        "        uniform float uFogFar;"
        "        uniform float uFogFar;\n        uniform float uFogPower;"
        ${Q1410_SHADER_VAR} "${${Q1410_SHADER_VAR}}")
    string(REPLACE
        "            float fogFactor = smoothstep(uFogNear, max(uFogFar, uFogNear + 0.01), fogDistance);"
        "            float q1410FogT = clamp((fogDistance - uFogNear) / max(uFogFar - uFogNear, 0.01), 0.0, 1.0);\n            float fogFactor = pow(q1410FogT, max(uFogPower, 0.001));"
        ${Q1410_SHADER_VAR} "${${Q1410_SHADER_VAR}}")
endforeach()

# Static uniform handle/upload.
string(REPLACE
    "GLint gFogFarLocationQ1010 = -1;"
    "GLint gFogFarLocationQ1010 = -1;\nGLint gFogPowerLocationQ1410 = -1;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    gFogFarLocationQ1010 = glGetUniformLocation(gProgram, \"uFogFar\");"
    "    gFogFarLocationQ1010 = glGetUniformLocation(gProgram, \"uFogFar\");\n    gFogPowerLocationQ1410 = glGetUniformLocation(gProgram, \"uFogPower\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    if (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f) {"
    "    glUniform1f(gFogPowerLocationQ1410, std::max(0.001f, fo3todq1400::gFogPowerQ1410));\n    if (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f) {"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# LAND uniform handle/upload.
string(REPLACE
    "GLint q1010TerrainFogFarLocation = -1;"
    "GLint q1010TerrainFogFarLocation = -1;\nGLint q1410TerrainFogPowerLocation = -1;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q1010TerrainFogFarLocation = glGetUniformLocation(q76bProgram, \"uFogFar\");"
    "    q1010TerrainFogFarLocation = glGetUniformLocation(q76bProgram, \"uFogFar\");\n    q1410TerrainFogPowerLocation = glGetUniformLocation(q76bProgram, \"uFogPower\");"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    if (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f) {"
    "    glUniform1f(q1410TerrainFogPowerLocation, std::max(0.001f, fo3todq1400::gFogPowerQ1410));\n    if (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f) {"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "uniform float uFogPower;" Q1410_STATIC_FOG_UNIFORM_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1410FogT" Q1410_STATIC_FOG_CURVE_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "uniform float uFogPower;" Q1410_LAND_FOG_UNIFORM_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "q1410FogT" Q1410_LAND_FOG_CURVE_OK)
if(Q1410_STATIC_FOG_UNIFORM_OK EQUAL -1 OR Q1410_STATIC_FOG_CURVE_OK EQUAL -1 OR
   Q1410_LAND_FOG_UNIFORM_OK EQUAL -1 OR Q1410_LAND_FOG_CURVE_OK EQUAL -1)
    message(FATAL_ERROR
        "Q14.1 fog shader hook drifted: staticUniform=${Q1410_STATIC_FOG_UNIFORM_OK} staticCurve=${Q1410_STATIC_FOG_CURVE_OK} landUniform=${Q1410_LAND_FOG_UNIFORM_OK} landCurve=${Q1410_LAND_FOG_CURVE_OK}")
endif()

# Rewrite both generated renderer translation units after the final Q14.1 transforms.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")

message(STATUS "Q14.1 Megaton CELL XCLR/RDWT weather + XCIM ImageSpace + WTHR Fog Power enabled; Q13.9 colour transfer unchanged")
