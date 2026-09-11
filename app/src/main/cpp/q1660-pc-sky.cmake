# Q15.16: port the PC-captured Fallout 3 SKY/SKYTEX semantics without touching
# Q15.15's now-good world/material/HDR/output path.
#
# Captured Megaton noon ground truth:
#   * SKY blends raw WTHR Horizon/SkyLower/SkyUpper byte/255 values.
#   * SKY/SKYTEX multiplies RGB by Params.y = 1.55; sRGB writes are disabled.
#   * Sun is a textured SRC_ALPHA/ONE additive pass, not a procedural disc.
#   * Clouds animate their authored V coordinate through TexCoordYOff.
#   * WastelandCloudHorizon01 is visibly drawn with BlendColor alpha 1.0 even
#     though its WTHR PNAM Day alpha byte is zero. PNAM alpha is not visibility.
#
# Use small deterministic source replacements here. The first Q15.16 version
# used one large ApplySky text block and CI correctly rejected it when comments
# made the exact anchor differ; these replacements intentionally avoid that.

# -----------------------------------------------------------------------------
# Q10.0 sky gradient: raw WTHR blend * captured SKY Params.y (1.55).
# -----------------------------------------------------------------------------
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-environment-q1000.h" Q1660_ENV_SOURCE)
set(Q1660_GRADIENT_OLD "            fragColor = vec4(colour, 1.0);")
set(Q1660_GRADIENT_NEW [=[
            // Q15.16: PC SKY ps c4.y at the captured Megaton noon draw.
            fragColor = vec4(colour * 1.55, 1.0);
]=])
string(FIND "${Q1660_ENV_SOURCE}" "${Q1660_GRADIENT_OLD}" Q1660_GRADIENT_POS)
if(Q1660_GRADIENT_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q10.0 sky-gradient output")
endif()
string(REPLACE "${Q1660_GRADIENT_OLD}" "${Q1660_GRADIENT_NEW}"
       Q1660_ENV_SOURCE "${Q1660_ENV_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-environment-q1000.h" "${Q1660_ENV_SOURCE}")

# -----------------------------------------------------------------------------
# Q14.0 time of day: keep the world/fog lighting in its existing linear path,
# but interpolate SKY/SKYTEX colours in their raw authored byte/display domain.
# -----------------------------------------------------------------------------
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-time-of-day-q1400.h" Q1660_TOD_SOURCE)

set(Q1660_SAMPLE_ANCHOR "inline float LinearToSrgb(float value) {")
set(Q1660_SAMPLE_INSERT [=[
inline void SampleEncodedRgb(const WeatherTimeQ1400& weather, int cls,
                             const TimeWeightsQ1400& weights, float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    if (!weather.haveNam0 || cls < 0 || cls >= 10) return;
    for (int tod = 0; tod < 4; ++tod) {
        const float w = weights.w[tod];
        for (int c = 0; c < 3; ++c) {
            out[c] += weather.encoded[cls][tod][c] * w;
        }
    }
}

inline float LinearToSrgb(float value) {
]=])
string(FIND "${Q1660_TOD_SOURCE}" "${Q1660_SAMPLE_ANCHOR}" Q1660_SAMPLE_POS)
if(Q1660_SAMPLE_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q14.0 sampling-helper anchor")
endif()
string(REPLACE "${Q1660_SAMPLE_ANCHOR}" "${Q1660_SAMPLE_INSERT}"
       Q1660_TOD_SOURCE "${Q1660_TOD_SOURCE}")

# ApplyEnvironment(): only sky/sun display-domain classes change. Ambient,
# sunlight and fog remain exactly on Q15.15's proven world-light path.
set(Q1660_SKYUP_OLD [=[
    SampleLinearRgb(runtime.weather, 0, weights, value);
    for (int c = 0; c < 3; ++c) env.skyUpper[c] = value[c] * gFo3ImageSpaceQ1280.hdrLumRampNoTex;
]=])
set(Q1660_SKYUP_NEW [=[
    SampleEncodedRgb(runtime.weather, 0, weights, value);
    for (int c = 0; c < 3; ++c) env.skyUpper[c] = value[c];
]=])
set(Q1660_SUN_OLD "    SampleLinearRgb(runtime.weather, 5, weights, value);")
set(Q1660_SUN_NEW "    SampleEncodedRgb(runtime.weather, 5, weights, value);")
set(Q1660_SKYLOW_OLD [=[
    SampleLinearRgb(runtime.weather, 7, weights, value);
    for (int c = 0; c < 3; ++c) env.skyLower[c] = value[c] * gFo3ImageSpaceQ1280.hdrLumRampNoTex;
]=])
set(Q1660_SKYLOW_NEW [=[
    SampleEncodedRgb(runtime.weather, 7, weights, value);
    for (int c = 0; c < 3; ++c) env.skyLower[c] = value[c];
]=])
set(Q1660_HORIZON_OLD [=[
    SampleLinearRgb(runtime.weather, 8, weights, value);
    for (int c = 0; c < 3; ++c) env.horizon[c] = value[c] * gFo3ImageSpaceQ1280.hdrLumRampNoTex;
]=])
set(Q1660_HORIZON_NEW [=[
    SampleEncodedRgb(runtime.weather, 8, weights, value);
    for (int c = 0; c < 3; ++c) env.horizon[c] = value[c];
]=])
foreach(pair IN ITEMS SKYUP SKYLOW HORIZON)
    string(FIND "${Q1660_TOD_SOURCE}" "${Q1660_${pair}_OLD}" Q1660_${pair}_POS)
    if(Q1660_${pair}_POS EQUAL -1)
        message(FATAL_ERROR "Q15.16 could not find Q14.0 ${pair} sampling block")
    endif()
    string(REPLACE "${Q1660_${pair}_OLD}" "${Q1660_${pair}_NEW}"
           Q1660_TOD_SOURCE "${Q1660_TOD_SOURCE}")
endforeach()
# Class 5 appears once in ApplyEnvironment and once in ApplySky. Replacing both
# is intentional: Sun is a SKY/SKYTEX display-domain colour in both locations.
string(REPLACE "${Q1660_SUN_OLD}" "${Q1660_SUN_NEW}"
       Q1660_TOD_SOURCE "${Q1660_TOD_SOURCE}")

# ApplySky(): raw PNAM RGB; do not use PNAM alpha as layer visibility.
set(Q1660_CLOUD_RGB_OLD [=[
                    rgb[c] += fo3colorq1390::SrgbToLinear(
                        runtime.weather.cloudEncoded[layer][tod][c]) * weights.w[tod];
]=])
set(Q1660_CLOUD_RGB_NEW [=[
                    rgb[c] += runtime.weather.cloudEncoded[layer][tod][c] * weights.w[tod];
]=])
string(FIND "${Q1660_TOD_SOURCE}" "${Q1660_CLOUD_RGB_OLD}" Q1660_CLOUD_RGB_POS)
if(Q1660_CLOUD_RGB_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q14.0 cloud RGB interpolation")
endif()
string(REPLACE "${Q1660_CLOUD_RGB_OLD}" "${Q1660_CLOUD_RGB_NEW}"
       Q1660_TOD_SOURCE "${Q1660_TOD_SOURCE}")

set(Q1660_CLOUD_ALPHA_OLD "            gWeatherSkyQ1330.clouds[layer].color[3] = Clamp01(alpha);")
set(Q1660_CLOUD_ALPHA_NEW [=[
            // PC draw has BlendColor alpha=1 for WastelandCloudHorizon01 even
            // though WastelandClearMegaton PNAM Day alpha is authored as zero.
            gWeatherSkyQ1330.clouds[layer].color[3] = 1.0f;
]=])
string(FIND "${Q1660_TOD_SOURCE}" "${Q1660_CLOUD_ALPHA_OLD}" Q1660_CLOUD_ALPHA_POS)
if(Q1660_CLOUD_ALPHA_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q14.0 cloud alpha assignment")
endif()
string(REPLACE "${Q1660_CLOUD_ALPHA_OLD}" "${Q1660_CLOUD_ALPHA_NEW}"
       Q1660_TOD_SOURCE "${Q1660_TOD_SOURCE}")

# We are intentionally keeping raw sky values from Q13.9's later authored-colour
# decoder. gSkyConverted is therefore still true: here it means 'ready for SKY'.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-time-of-day-q1400.h" "${Q1660_TOD_SOURCE}")

# -----------------------------------------------------------------------------
# Q13.3 SKYTEX: authored sun texture, V-scroll clouds, captured RGB scale.
# -----------------------------------------------------------------------------
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-weather-sky-q1330.h" Q1660_SKY_SOURCE)

set(Q1660_GLOBAL_OLD [=[
inline GLint gSunGlareLocQ1330 = -1;
inline bool gLoggedGpuQ1330 = false;
]=])
set(Q1660_GLOBAL_NEW [=[
inline GLint gSunGlareLocQ1330 = -1;
inline GLint gSunTexLocQ1330 = -1;
inline GLint gUseSunTexLocQ1330 = -1;
inline GLuint gSunTextureQ1660 = 0u;
inline bool gSunTextureAttemptedQ1660 = false;
inline bool gLoggedGpuQ1330 = false;
]=])
string(FIND "${Q1660_SKY_SOURCE}" "${Q1660_GLOBAL_OLD}" Q1660_GLOBAL_POS)
if(Q1660_GLOBAL_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q13.3 sky globals")
endif()
string(REPLACE "${Q1660_GLOBAL_OLD}" "${Q1660_GLOBAL_NEW}"
       Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

# Zero PNAM alpha does not prevent the PC cloud texture from existing/drawing.
string(REPLACE
    "if (layer.texturePath.empty() || layer.color[3] <= 0.001f) return false;"
    "if (layer.texturePath.empty()) return false;"
    Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")
string(REPLACE
    "if (!layer.loaded || layer.texture == 0u || layer.color[3] <= 0.001f) continue;"
    "if (!layer.loaded || layer.texture == 0u) continue;"
    Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

set(Q1660_SUN_LOADER_ANCHOR "inline bool EnsureWeatherQ1330() {")
set(Q1660_SUN_LOADER [=[
inline bool EnsureSunTextureQ1660() {
    if (gSunTextureQ1660 != 0u) return true;
    if (gSunTextureAttemptedQ1660) return false;
    gSunTextureAttemptedQ1660 = true;

    Fo3RgbaTexture decoded;
    if (!LoadFalloutTextureRgba("Sky\\Sun.dds", decoded) ||
        decoded.width <= 0 || decoded.height <= 0 || decoded.rgba.empty()) {
        __android_log_print(ANDROID_LOG_WARN, fo3envq1000::TAG,
                            "Q15.16 SUN DDS MISS: path=Sky\\\\Sun.dds fallback=procedural");
        return false;
    }
    glGenTextures(1, &gSunTextureQ1660);
    glBindTexture(GL_TEXTURE_2D, gSunTextureQ1660);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 decoded.width, decoded.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, decoded.rgba.data());
    __android_log_print(ANDROID_LOG_INFO, fo3envq1000::TAG,
                        "Q15.16 SUN DDS READY: path=Sky\\\\Sun.dds resolved=%s size=%dx%d format=%s",
                        decoded.sourcePath.c_str(), decoded.width, decoded.height,
                        decoded.format.c_str());
    return true;
}

inline bool EnsureWeatherQ1330() {
]=])
string(FIND "${Q1660_SKY_SOURCE}" "${Q1660_SUN_LOADER_ANCHOR}" Q1660_SUN_LOADER_POS)
if(Q1660_SUN_LOADER_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q13.3 EnsureWeather anchor")
endif()
string(REPLACE "${Q1660_SUN_LOADER_ANCHOR}" "${Q1660_SUN_LOADER}"
       Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

# Load the authored Sun after weather cloud uploads while a valid GLES context is
# guaranteed. No log-format surgery is needed; the loader has its own Q15.16 log.
set(Q1660_WEATHER_BIND_OLD [=[
    glBindTexture(GL_TEXTURE_2D, 0u);

    __android_log_print(
]=])
set(Q1660_WEATHER_BIND_NEW [=[
    (void)EnsureSunTextureQ1660();
    glBindTexture(GL_TEXTURE_2D, 0u);

    __android_log_print(
]=])
string(FIND "${Q1660_SKY_SOURCE}" "${Q1660_WEATHER_BIND_OLD}" Q1660_WEATHER_BIND_POS)
if(Q1660_WEATHER_BIND_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q13.3 post-weather texture bind")
endif()
string(REPLACE "${Q1660_WEATHER_BIND_OLD}" "${Q1660_WEATHER_BIND_NEW}"
       Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

set(Q1660_UNIFORM_OLD "        uniform sampler2D uCloud;\n        uniform vec4 uCloudColor;")
set(Q1660_UNIFORM_NEW "        uniform sampler2D uCloud;\n        uniform sampler2D uSunTex;\n        uniform int uUseSunTex;\n        uniform vec4 uCloudColor;")
string(FIND "${Q1660_SKY_SOURCE}" "${Q1660_UNIFORM_OLD}" Q1660_UNIFORM_POS)
if(Q1660_UNIFORM_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q13.3 sampler uniforms")
endif()
string(REPLACE "${Q1660_UNIFORM_OLD}" "${Q1660_UNIFORM_NEW}"
       Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

# Replace only the procedural Sun body. The authored texture itself contains the
# bright disc and wide halo seen in the PC state dump; keep a fallback if absent.
set(Q1660_SUN_BODY_OLD [=[
            if (uMode == 0) {
                float alignment = max(dot(d, normalize(uSunDirection)), 0.0);
                float disc = smoothstep(0.9978, 0.99965, alignment);
                float halo = pow(alignment, 72.0) * clamp(uSunGlare, 0.0, 1.0);
                float alpha = clamp(disc + halo * 0.42, 0.0, 1.0);
                vec3 colour = uSunColor * (disc * 1.30 + halo * 0.45);
                if (alpha <= 0.001) discard;
                fragColor = vec4(colour, alpha);
                return;
            }
]=])
set(Q1660_SUN_BODY_NEW [=[
            if (uMode == 0) {
                vec3 sunDir = normalize(uSunDirection);
                float alignment = dot(d, sunDir);
                if (uUseSunTex != 0 && alignment > 0.25) {
                    vec3 referenceUp = abs(sunDir.y) > 0.96
                        ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
                    vec3 right = normalize(cross(referenceUp, sunDir));
                    vec3 up = normalize(cross(sunDir, right));
                    // Wide dome projection: Sky\\Sun.dds already carries the
                    // compact disc plus the broad PC halo in its alpha channel.
                    const float halfSpan = 0.78;
                    vec2 uv = vec2(0.5) + vec2(dot(d, right), dot(d, up)) /
                                           (2.0 * halfSpan);
                    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) discard;
                    vec4 texel = texture(uSunTex, uv);
                    if (texel.a <= 0.001) discard;
                    fragColor = vec4(texel.rgb * uSunColor * 1.55, texel.a);
                    return;
                }

                alignment = max(alignment, 0.0);
                float disc = smoothstep(0.9978, 0.99965, alignment);
                float halo = pow(alignment, 72.0) * clamp(uSunGlare, 0.0, 1.0);
                float alpha = clamp(disc + halo * 0.42, 0.0, 1.0);
                vec3 colour = uSunColor * (disc * 1.30 + halo * 0.45) * 1.55;
                if (alpha <= 0.001) discard;
                fragColor = vec4(colour, alpha);
                return;
            }
]=])
string(FIND "${Q1660_SKY_SOURCE}" "${Q1660_SUN_BODY_OLD}" Q1660_SUN_BODY_POS)
if(Q1660_SUN_BODY_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q13.3 procedural Sun body")
endif()
string(REPLACE "${Q1660_SUN_BODY_OLD}" "${Q1660_SUN_BODY_NEW}"
       Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

# PC SKYTEX VS adds TexCoordYOff to V/Y, and PS multiplies texture*BlendColor by
# Params.y=1.55. Keep our sphere geometry for this milestone, but match both.
set(Q1660_CLOUD_UV_OLD "            vec2 uv = vec2(longitude + uCloudOffset, latitude * 1.65);")
set(Q1660_CLOUD_UV_NEW "            vec2 uv = vec2(longitude, latitude * 1.65 + uCloudOffset);")
set(Q1660_CLOUD_OUT_OLD "            fragColor = vec4(texel.rgb * uCloudColor.rgb, alpha);")
set(Q1660_CLOUD_OUT_NEW "            fragColor = vec4(texel.rgb * uCloudColor.rgb * 1.55, alpha);")
foreach(pair IN ITEMS CLOUD_UV CLOUD_OUT)
    string(FIND "${Q1660_SKY_SOURCE}" "${Q1660_${pair}_OLD}" Q1660_${pair}_POS)
    if(Q1660_${pair}_POS EQUAL -1)
        message(FATAL_ERROR "Q15.16 could not find Q13.3 ${pair} shader line")
    endif()
    string(REPLACE "${Q1660_${pair}_OLD}" "${Q1660_${pair}_NEW}"
           Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")
endforeach()

set(Q1660_LOC_OLD "    gCloudTexLocQ1330 = glGetUniformLocation(gProgramQ1330, \"uCloud\");")
set(Q1660_LOC_NEW "    gCloudTexLocQ1330 = glGetUniformLocation(gProgramQ1330, \"uCloud\");\n    gSunTexLocQ1330 = glGetUniformLocation(gProgramQ1330, \"uSunTex\");\n    gUseSunTexLocQ1330 = glGetUniformLocation(gProgramQ1330, \"uUseSunTex\");")
string(FIND "${Q1660_SKY_SOURCE}" "${Q1660_LOC_OLD}" Q1660_LOC_POS)
if(Q1660_LOC_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q13.3 uniform-location anchor")
endif()
string(REPLACE "${Q1660_LOC_OLD}" "${Q1660_LOC_NEW}"
       Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

set(Q1660_SUN_DRAW_OLD [=[
    glUniform1i(gModeLocQ1330, 0);
    glUniform3fv(gSunDirectionLocQ1330, 1, env.sunDirection);
    glUniform3fv(gSunColorLocQ1330, 1, gWeatherSkyQ1330.sunColor);
    glUniform1f(gSunGlareLocQ1330, gWeatherSkyQ1330.sunGlare);
    glDrawArrays(GL_TRIANGLES, 0, fo3envq1000::skyVertexCount);
]=])
set(Q1660_SUN_DRAW_NEW [=[
    glUniform1i(gModeLocQ1330, 0);
    glUniform3fv(gSunDirectionLocQ1330, 1, env.sunDirection);
    glUniform3fv(gSunColorLocQ1330, 1, gWeatherSkyQ1330.sunColor);
    glUniform1f(gSunGlareLocQ1330, gWeatherSkyQ1330.sunGlare);
    glUniform1i(gSunTexLocQ1330, 0);
    glUniform1i(gUseSunTexLocQ1330, gSunTextureQ1660 != 0u ? 1 : 0);
    if (gSunTextureQ1660 != 0u) glBindTexture(GL_TEXTURE_2D, gSunTextureQ1660);
    glDrawArrays(GL_TRIANGLES, 0, fo3envq1000::skyVertexCount);
]=])
string(FIND "${Q1660_SKY_SOURCE}" "${Q1660_SUN_DRAW_OLD}" Q1660_SUN_DRAW_POS)
if(Q1660_SUN_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q13.3 Sun draw")
endif()
string(REPLACE "${Q1660_SUN_DRAW_OLD}" "${Q1660_SUN_DRAW_NEW}"
       Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-weather-sky-q1330.h" "${Q1660_SKY_SOURCE}")

# -----------------------------------------------------------------------------
# Visible build identity: Q15.15 -> Q15.16.
# -----------------------------------------------------------------------------
set(Q1660_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1660_Q4_INPUT}")
    message(FATAL_ERROR "Q15.16 expected final OpenXR source at ${Q1660_Q4_INPUT}")
endif()
file(READ "${Q1660_Q4_INPUT}" Q1660_Q4_SOURCE)
string(REPLACE
    "q1600Digit(q1600X, 0x6Du); // 5 = A F G C D"
    "q1600Digit(q1600X, 0x7Du); // 6 = A F G E D C"
    Q1660_Q4_SOURCE "${Q1660_Q4_SOURCE}")
string(REPLACE "Q15.15" "Q15.16" Q1660_Q4_SOURCE "${Q1660_Q4_SOURCE}")
file(WRITE "${Q1660_Q4_INPUT}" "${Q1660_Q4_SOURCE}")

# Hard guards: sky changes present; world colour baseline untouched.
string(FIND "${Q1660_ENV_SOURCE}" "colour * 1.55" Q1660_GRADIENT_OK)
string(FIND "${Q1660_TOD_SOURCE}" "SampleEncodedRgb(runtime.weather, 0" Q1660_RAW_SKY_OK)
string(FIND "${Q1660_TOD_SOURCE}" "clouds[layer].color[3] = 1.0f" Q1660_CLOUD_ALPHA_OK)
string(FIND "${Q1660_SKY_SOURCE}" "Sky\\\\Sun.dds" Q1660_SUN_TEX_OK)
string(FIND "${Q1660_SKY_SOURCE}" "latitude * 1.65 + uCloudOffset" Q1660_CLOUD_V_OK)
string(FIND "${Q1660_SKY_SOURCE}" "uCloudColor.rgb * 1.55" Q1660_CLOUD_SCALE_OK)
string(FIND "${Q1660_Q4_SOURCE}" "text=Q15.16 anchor=left-hand" Q1660_LABEL_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "colour = q1640PcOutput;" Q1660_Q1515_BASELINE_OK)
if(Q1660_GRADIENT_OK EQUAL -1 OR Q1660_RAW_SKY_OK EQUAL -1 OR
   Q1660_CLOUD_ALPHA_OK EQUAL -1 OR Q1660_SUN_TEX_OK EQUAL -1 OR
   Q1660_CLOUD_V_OK EQUAL -1 OR Q1660_CLOUD_SCALE_OK EQUAL -1 OR
   Q1660_LABEL_OK EQUAL -1 OR Q1660_Q1515_BASELINE_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.16 verification failed: gradient=${Q1660_GRADIENT_OK} rawSky=${Q1660_RAW_SKY_OK} cloudAlpha=${Q1660_CLOUD_ALPHA_OK} sunTex=${Q1660_SUN_TEX_OK} cloudV=${Q1660_CLOUD_V_OK} cloudScale=${Q1660_CLOUD_SCALE_OK} label=${Q1660_LABEL_OK} q1515=${Q1660_Q1515_BASELINE_OK}")
endif()

message(STATUS "Q15.16 PC-captured sky enabled: raw WTHR*1.55, PNAM-zero cloud visible, textured additive Sun, V-scroll")
