# Q15.16: replace the remaining homemade sky colour/visibility semantics with
# behaviour proven by the uploaded PC D3D9 captures.
#
# Ground truth from the Megaton noon sky draws:
#   * SKY gradient VS blends raw WTHR Horizon/SkyLower/SkyUpper byte/255 values.
#   * SKY/SKYTEX PS multiplies RGB by Params.y = 1.55; SRGBWRITEENABLE is off.
#   * Sun uses a textured additive pass (SRC_ALPHA, ONE), not a procedural disc.
#   * Cloud texture coordinates animate in V/Y (TexCoordYOff), not longitude/U.
#   * Visible WastelandCloudHorizon01 has vertex/BlendColor alpha 1 even though
#     its WTHR PNAM Day alpha byte is 0. PNAM alpha therefore must not gate it.
#
# Q15.15's now-good world/material/HDR/output path is deliberately untouched.
# We shadow only the three sky/time headers from the binary directory and bump
# the visible left-hand build label to Q15.16.

# -----------------------------------------------------------------------------
# Q10.0 base gradient: PC applies the captured SKY Params.y RGB scale after the
# three authored colours have been blended. Keep the authored values raw here.
# -----------------------------------------------------------------------------
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-environment-q1000.h" Q1660_ENV_SOURCE)
set(Q1660_ENV_OLD [=[
            vec3 colour = mix(lower, uSkyUpper, smoothstep(0.28, 0.95, y));
            fragColor = vec4(colour, 1.0);
]=])
set(Q1660_ENV_NEW [=[
            vec3 colour = mix(lower, uSkyUpper, smoothstep(0.28, 0.95, y));
            // Q15.16: PC SKY ps c4.y at the captured Megaton noon draw.
            // The three WTHR RGB endpoints are raw byte/255 values here.
            fragColor = vec4(colour * 1.55, 1.0);
]=])
string(FIND "${Q1660_ENV_SOURCE}" "${Q1660_ENV_OLD}" Q1660_ENV_POS)
if(Q1660_ENV_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q10.0 sky-gradient output")
endif()
string(REPLACE "${Q1660_ENV_OLD}" "${Q1660_ENV_NEW}"
       Q1660_ENV_SOURCE "${Q1660_ENV_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-environment-q1000.h" "${Q1660_ENV_SOURCE}")

# -----------------------------------------------------------------------------
# Q14.0 time-of-day: the PC SKY/SKYTEX path consumes raw authored colour bytes,
# unlike the linear world-lighting path. Add a raw interpolation helper and use
# it only for sky upper/lower/horizon, Sun and PNAM cloud RGB.
# -----------------------------------------------------------------------------
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-time-of-day-q1400.h" Q1660_TOD_SOURCE)
set(Q1660_SAMPLE_ANCHOR [=[
inline float LinearToSrgb(float value) {
]=])
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
    message(FATAL_ERROR "Q15.16 could not find Q14.0 sampling helper anchor")
endif()
string(REPLACE "${Q1660_SAMPLE_ANCHOR}" "${Q1660_SAMPLE_INSERT}"
       Q1660_TOD_SOURCE "${Q1660_TOD_SOURCE}")

set(Q1660_ENV_TIME_OLD [=[
    float value[3]{};
    SampleLinearRgb(runtime.weather, 0, weights, value);
    for (int c = 0; c < 3; ++c) env.skyUpper[c] = value[c] * gFo3ImageSpaceQ1280.hdrLumRampNoTex;
    SampleLinearRgb(runtime.weather, 1, weights, value);
    for (int c = 0; c < 3; ++c) env.fog[c] = value[c];
    SampleLinearRgb(runtime.weather, 3, weights, value);
    for (int c = 0; c < 3; ++c) env.ambient[c] = value[c];
    SampleLinearRgb(runtime.weather, 4, weights, value);
    const float sunlightScale = fo3weatherq1320::EffectiveSunlightScaleQ1320(
        gFo3ImageSpaceQ1280.hdrSunlightDimmer);
    for (int c = 0; c < 3; ++c) env.sunlight[c] = value[c] * sunlightScale;
    SampleLinearRgb(runtime.weather, 5, weights, value);
    for (int c = 0; c < 3; ++c) env.sun[c] = value[c];
    SampleLinearRgb(runtime.weather, 7, weights, value);
    for (int c = 0; c < 3; ++c) env.skyLower[c] = value[c] * gFo3ImageSpaceQ1280.hdrLumRampNoTex;
    SampleLinearRgb(runtime.weather, 8, weights, value);
    for (int c = 0; c < 3; ++c) env.horizon[c] = value[c] * gFo3ImageSpaceQ1280.hdrLumRampNoTex;
]=])
set(Q1660_ENV_TIME_NEW [=[
    float value[3]{};
    // Q15.16: PC SKY uses raw WTHR byte/255 RGB and applies its 1.55 scale in
    // the pixel shader. Do not sRGB-decode or apply Q14.0's guessed skyScale.
    SampleEncodedRgb(runtime.weather, 0, weights, value);
    for (int c = 0; c < 3; ++c) env.skyUpper[c] = value[c];
    SampleLinearRgb(runtime.weather, 1, weights, value);
    for (int c = 0; c < 3; ++c) env.fog[c] = value[c];
    SampleLinearRgb(runtime.weather, 3, weights, value);
    for (int c = 0; c < 3; ++c) env.ambient[c] = value[c];
    SampleLinearRgb(runtime.weather, 4, weights, value);
    const float sunlightScale = fo3weatherq1320::EffectiveSunlightScaleQ1320(
        gFo3ImageSpaceQ1280.hdrSunlightDimmer);
    for (int c = 0; c < 3; ++c) env.sunlight[c] = value[c] * sunlightScale;
    SampleEncodedRgb(runtime.weather, 5, weights, value);
    for (int c = 0; c < 3; ++c) env.sun[c] = value[c];
    SampleEncodedRgb(runtime.weather, 7, weights, value);
    for (int c = 0; c < 3; ++c) env.skyLower[c] = value[c];
    SampleEncodedRgb(runtime.weather, 8, weights, value);
    for (int c = 0; c < 3; ++c) env.horizon[c] = value[c];
]=])
string(FIND "${Q1660_TOD_SOURCE}" "${Q1660_ENV_TIME_OLD}" Q1660_ENV_TIME_POS)
if(Q1660_ENV_TIME_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q14.0 sky environment sampling block")
endif()
string(REPLACE "${Q1660_ENV_TIME_OLD}" "${Q1660_ENV_TIME_NEW}"
       Q1660_TOD_SOURCE "${Q1660_TOD_SOURCE}")

set(Q1660_APPLYSKY_OLD [=[
    float sun[3]{};
    SampleLinearRgb(runtime.weather, 5, weights, sun);
    for (int c = 0; c < 3; ++c) gWeatherSkyQ1330.sunColor[c] = sun[c];

    if (runtime.weather.havePnam) {
        for (int layer = 0; layer < 4; ++layer) {
            float rgb[3]{0.0f, 0.0f, 0.0f};
            float alpha = 0.0f;
            for (int tod = 0; tod < 4; ++tod) {
                for (int c = 0; c < 3; ++c) {
                    rgb[c] += fo3colorq1390::SrgbToLinear(
                        runtime.weather.cloudEncoded[layer][tod][c]) * weights.w[tod];
                }
                alpha += runtime.weather.cloudEncoded[layer][tod][3] * weights.w[tod];
            }
            for (int c = 0; c < 3; ++c) gWeatherSkyQ1330.clouds[layer].color[c] = rgb[c];
            gWeatherSkyQ1330.clouds[layer].color[3] = Clamp01(alpha);
        }
    }
]=])
set(Q1660_APPLYSKY_NEW [=[
    float sun[3]{};
    SampleEncodedRgb(runtime.weather, 5, weights, sun);
    for (int c = 0; c < 3; ++c) gWeatherSkyQ1330.sunColor[c] = sun[c];

    if (runtime.weather.havePnam) {
        for (int layer = 0; layer < 4; ++layer) {
            float rgb[3]{0.0f, 0.0f, 0.0f};
            for (int tod = 0; tod < 4; ++tod) {
                for (int c = 0; c < 3; ++c) {
                    rgb[c] += runtime.weather.cloudEncoded[layer][tod][c] * weights.w[tod];
                }
            }
            for (int c = 0; c < 3; ++c) gWeatherSkyQ1330.clouds[layer].color[c] = rgb[c];
            // PC BlendColor alpha is 1.0 for the captured visible cloud even
            // though WastelandClearMegaton PNAM Day alpha is authored as zero.
            gWeatherSkyQ1330.clouds[layer].color[3] = 1.0f;
        }
    }
]=])
string(FIND "${Q1660_TOD_SOURCE}" "${Q1660_APPLYSKY_OLD}" Q1660_APPLYSKY_POS)
if(Q1660_APPLYSKY_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q14.0 weather-sky sampling block")
endif()
string(REPLACE "${Q1660_APPLYSKY_OLD}" "${Q1660_APPLYSKY_NEW}"
       Q1660_TOD_SOURCE "${Q1660_TOD_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-time-of-day-q1400.h" "${Q1660_TOD_SOURCE}")

# -----------------------------------------------------------------------------
# Q13.3 weather sky: load Bethesda's actual Sky\\Sun.dds, use the texture's
# authored disc/halo alpha with PC additive blending, animate clouds along V,
# and apply the captured 1.55 RGB scale in the SKYTEX pixel path.
# -----------------------------------------------------------------------------
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-weather-sky-q1330.h" Q1660_SKY_SOURCE)

string(REPLACE
    "inline GLint gSunGlareLocQ1330 = -1;\ninline bool gLoggedGpuQ1330 = false;"
    "inline GLint gSunGlareLocQ1330 = -1;\ninline GLint gSunTexLocQ1330 = -1;\ninline GLint gUseSunTexLocQ1330 = -1;\ninline GLuint gSunTextureQ1660 = 0u;\ninline bool gSunTextureAttemptedQ1660 = false;\ninline bool gLoggedGpuQ1330 = false;"
    Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

string(REPLACE
    "if (layer.texturePath.empty() || layer.color[3] <= 0.001f) return false;"
    "if (layer.texturePath.empty()) return false;"
    Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

set(Q1660_SUN_UPLOAD_ANCHOR [=[
inline bool EnsureWeatherQ1330() {
]=])
set(Q1660_SUN_UPLOAD [=[
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
string(FIND "${Q1660_SKY_SOURCE}" "${Q1660_SUN_UPLOAD_ANCHOR}" Q1660_SUN_UPLOAD_POS)
if(Q1660_SUN_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q13.3 weather setup anchor")
endif()
string(REPLACE "${Q1660_SUN_UPLOAD_ANCHOR}" "${Q1660_SUN_UPLOAD}"
       Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

# Make the texture available before Q13.3 drops its temporary texture binding.
string(REPLACE
    "    glBindTexture(GL_TEXTURE_2D, 0u);\n\n    __android_log_print("
    "    const bool q1660SunTextureReady = EnsureSunTextureQ1660();\n    glBindTexture(GL_TEXTURE_2D, 0u);\n\n    __android_log_print("
    Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")
string(REPLACE
    "sunGlare=%.3f mode=DAY\","
    "sunGlare=%.3f mode=DAY q1516SunTexture=%d\","
    Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")
string(REPLACE
    "gWeatherSkyQ1330.sunColor[2], gWeatherSkyQ1330.sunGlare);"
    "gWeatherSkyQ1330.sunColor[2], gWeatherSkyQ1330.sunGlare, q1660SunTextureReady ? 1 : 0);"
    Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

string(REPLACE
    "        uniform sampler2D uCloud;\n        uniform vec4 uCloudColor;"
    "        uniform sampler2D uCloud;\n        uniform sampler2D uSunTex;\n        uniform int uUseSunTex;\n        uniform vec4 uCloudColor;"
    Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

set(Q1660_SKY_SHADER_OLD [=[
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

            float longitude = atan(d.z, d.x) / (2.0 * PI) + 0.5;
            float latitude = acos(clamp(d.y, -1.0, 1.0)) / PI;
            vec2 uv = vec2(longitude + uCloudOffset, latitude * 1.65);
            vec4 texel = texture(uCloud, uv);
            float horizonFade = smoothstep(-0.02, 0.18, d.y);
            float alpha = texel.a * uCloudColor.a * horizonFade;
            if (alpha <= 0.002) discard;
            fragColor = vec4(texel.rgb * uCloudColor.rgb, alpha);
]=])
set(Q1660_SKY_SHADER_NEW [=[
            if (uMode == 0) {
                vec3 sunDir = normalize(uSunDirection);
                float alignment = dot(d, sunDir);
                if (uUseSunTex != 0 && alignment > 0.25) {
                    // Reconstruct the PC's large textured sun billboard on the
                    // sky dome. Sky\\Sun.dds itself carries the small disc and
                    // broad halo in alpha; the captured pass then multiplies RGB
                    // by raw Sun WTHR colour and Params.y=1.55.
                    vec3 referenceUp = abs(sunDir.y) > 0.96
                        ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
                    vec3 right = normalize(cross(referenceUp, sunDir));
                    vec3 up = normalize(cross(sunDir, right));
                    const float halfSpan = 0.78;
                    vec2 uv = vec2(0.5) + vec2(dot(d, right), dot(d, up)) /
                                           (2.0 * halfSpan);
                    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) discard;
                    vec4 sunTexel = texture(uSunTex, uv);
                    float alpha = sunTexel.a;
                    if (alpha <= 0.001) discard;
                    fragColor = vec4(sunTexel.rgb * uSunColor * 1.55, alpha);
                    return;
                }

                // Texture-load fallback only; keep old disc so a missing BSA
                // asset cannot make the Sun disappear entirely.
                alignment = max(alignment, 0.0);
                float disc = smoothstep(0.9978, 0.99965, alignment);
                float halo = pow(alignment, 72.0) * clamp(uSunGlare, 0.0, 1.0);
                float alpha = clamp(disc + halo * 0.42, 0.0, 1.0);
                vec3 colour = uSunColor * (disc * 1.30 + halo * 0.45) * 1.55;
                if (alpha <= 0.001) discard;
                fragColor = vec4(colour, alpha);
                return;
            }

            float longitude = atan(d.z, d.x) / (2.0 * PI) + 0.5;
            float latitude = acos(clamp(d.y, -1.0, 1.0)) / PI;
            // PC SKYTEX adds TexCoordYOff to the authored V coordinate. Our
            // procedural dome still approximates the authored sky mesh, but the
            // animation axis now matches the captured vertex shader.
            vec2 uv = vec2(longitude, latitude * 1.65 + uCloudOffset);
            vec4 texel = texture(uCloud, uv);
            float horizonFade = smoothstep(-0.02, 0.18, d.y);
            float alpha = texel.a * uCloudColor.a * horizonFade;
            if (alpha <= 0.002) discard;
            fragColor = vec4(texel.rgb * uCloudColor.rgb * 1.55, alpha);
]=])
string(FIND "${Q1660_SKY_SOURCE}" "${Q1660_SKY_SHADER_OLD}" Q1660_SKY_SHADER_POS)
if(Q1660_SKY_SHADER_POS EQUAL -1)
    message(FATAL_ERROR "Q15.16 could not find Q13.3 procedural Sun/cloud shader block")
endif()
string(REPLACE "${Q1660_SKY_SHADER_OLD}" "${Q1660_SKY_SHADER_NEW}"
       Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

string(REPLACE
    "    gCloudTexLocQ1330 = glGetUniformLocation(gProgramQ1330, \"uCloud\");"
    "    gCloudTexLocQ1330 = glGetUniformLocation(gProgramQ1330, \"uCloud\");\n    gSunTexLocQ1330 = glGetUniformLocation(gProgramQ1330, \"uSunTex\");\n    gUseSunTexLocQ1330 = glGetUniformLocation(gProgramQ1330, \"uUseSunTex\");"
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
    message(FATAL_ERROR "Q15.16 could not find Q13.3 Sun draw block")
endif()
string(REPLACE "${Q1660_SUN_DRAW_OLD}" "${Q1660_SUN_DRAW_NEW}"
       Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

string(REPLACE
    "if (!layer.loaded || layer.texture == 0u || layer.color[3] <= 0.001f) continue;"
    "if (!layer.loaded || layer.texture == 0u) continue;"
    Q1660_SKY_SOURCE "${Q1660_SKY_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-weather-sky-q1330.h" "${Q1660_SKY_SOURCE}")

# -----------------------------------------------------------------------------
# Visible build identity: Q15.15 -> Q15.16.
# Seven-segment 6 = A F G E D C = 0x7D with Q15.10's bit layout.
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

# Hard guards: prove the PC sky semantics are present and that the Q15.15 world
# output patch remains in the native source untouched.
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

# Q15.16 changes only generated/shadow sky headers and the visible q4 label.
# q6h-native-generated.cpp still points at the same final eye source and keeps
# Q15.15's direct PC output-domain write.
message(STATUS "Q15.16 PC-captured sky enabled: raw WTHR*1.55, visible PNAM-zero cloud, textured additive Sun, V-scroll")
