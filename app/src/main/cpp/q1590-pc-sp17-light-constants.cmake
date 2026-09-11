# Q15.9: align static/NIF SP17 world-light constants with the captured PC D3D9 draw.
#
# PC Megaton capture (Shader Package 17 PPLighting):
#   AmbientColor / ps c1  = raw WTHR Ambient byte/255
#   PSLightColor / ps c3  = raw WTHR Sunlight byte/255 * (1 + base Sunlight Dimmer)
# At 12:00 in WastelandClearMegaton this is:
#   AmbientColor = (41,147,182)/255 = (0.160784,0.576471,0.713725)
#   PSLightColor = (255,206,170)/255 * 2.5 = (2.5,2.019608,1.666667)
#
# Q13.9 currently sRGB-decodes these WTHR bytes before the static shader sees
# them, and Q14.0's weather IMAD path also changes the sunlight dimmer. Both are
# directly contradicted by the captured SP17 constants for this draw. Q15.9 is
# deliberately narrow: only static/NIF PPLighting constants are overridden.
# LAND, sky, fog, BaseMap sampling, HDR/ImageSpace and sun direction are untouched.

# -----------------------------------------------------------------------------
# Build the exact raw WTHR constants for the current Q14.0 clock. We interpolate
# the authored byte/display-domain endpoints with the same CLMT time weights, but
# do NOT sRGB-decode Ambient/Sunlight. Directional scale comes from the base IMGS
# sunlight dimmer, before weather IMAD modification, matching the PC 2.5x noon
# capture when the authored dimmer is 1.5.
# -----------------------------------------------------------------------------
set(Q1590_HELPERS [=[
bool Q1590GetPcSp17LightConstants(float ambient[3], float sunlight[3],
                                  float& baseSunDimmer, float& effectiveSunScale) {
    using namespace fo3todq1400;
    if (!gRuntime.ready || !gRuntime.weather.haveNam0 || !gRuntime.climate.valid) {
        return false;
    }

    const TimeWeightsQ1400 weights = WeightsForHour(gTestHour, gRuntime.climate);
    float rawAmbient[3]{0.0f, 0.0f, 0.0f};
    float rawSunlight[3]{0.0f, 0.0f, 0.0f};
    for (int tod = 0; tod < 4; ++tod) {
        const float w = weights.w[tod];
        for (int c = 0; c < 3; ++c) {
            rawAmbient[c] += gRuntime.weather.encoded[3][tod][c] * w;
            rawSunlight[c] += gRuntime.weather.encoded[4][tod][c] * w;
        }
    }

    baseSunDimmer = gRuntime.baseImage.valid
        ? gRuntime.baseImage.hdrSunlightDimmer
        : 1.5f;
    effectiveSunScale = fo3weatherq1320::EffectiveSunlightScaleQ1320(baseSunDimmer);
    for (int c = 0; c < 3; ++c) {
        ambient[c] = rawAmbient[c];
        sunlight[c] = rawSunlight[c] * effectiveSunScale;
    }
    return true;
}

void Q1590UploadPcSp17LightConstants() {
    float ambient[3]{0.34f, 0.34f, 0.34f};
    float sunlight[3]{0.66f, 0.66f, 0.66f};
    float baseSunDimmer = 0.0f;
    float effectiveSunScale = 1.0f;
    if (!Q1590GetPcSp17LightConstants(
            ambient, sunlight, baseSunDimmer, effectiveSunScale)) {
        return;
    }

    if (gAmbientColorLocationQ1000 >= 0) {
        glUniform3fv(gAmbientColorLocationQ1000, 1, ambient);
    }
    if (gSunlightColorLocationQ1000 >= 0) {
        glUniform3fv(gSunlightColorLocationQ1000, 1, sunlight);
    }

    // Q15.7's encoded-BaseMap branch changes more than the PC capture proves.
    // Keep it disabled for statics so this test changes only the two proven
    // SP17 lighting constants. Terrain remains untouched by Q15.9.
    if (gLegacyColourDomainLocationQ1570 >= 0) {
        glUniform1f(gLegacyColourDomainLocationQ1570, 0.0f);
    }

    static uint32_t q1590LastWeather = 0u;
    static int q1590LastHour = -1;
    const int hourBucket = static_cast<int>(std::floor(fo3todq1400::gTestHour * 10.0f));
    if (q1590LastWeather != fo3todq1400::gRuntime.weatherFormId ||
        q1590LastHour != hourBucket) {
        q1590LastWeather = fo3todq1400::gRuntime.weatherFormId;
        q1590LastHour = hourBucket;
        Q6H_LOGI(
            "Q15.9 PC SP17 CONSTANTS: weather=%08X EDID=%s hour=%.2f ambient=(%.6f %.6f %.6f) sunlight=(%.6f %.6f %.6f) baseSunDimmer=%.3f effectiveSunScale=%.3f scope=static-PPLighting terrainChanged=0 baseMapChanged=0 sunDirectionChanged=0",
            fo3todq1400::gRuntime.weatherFormId,
            fo3todq1400::gRuntime.weather.editorId.empty()
                ? "<none>" : fo3todq1400::gRuntime.weather.editorId.c_str(),
            fo3todq1400::gTestHour,
            ambient[0], ambient[1], ambient[2],
            sunlight[0], sunlight[1], sunlight[2],
            baseSunDimmer, effectiveSunScale);
    }
}

]=])

set(Q1590_FUNCTION_ANCHOR [=[
void DrawSceneObject(const GpuObject& object) {
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1590_FUNCTION_ANCHOR}" Q1590_FUNCTION_POS)
if(Q1590_FUNCTION_POS EQUAL -1)
    message(FATAL_ERROR "Q15.9 could not find DrawSceneObject helper insertion point")
endif()
string(REPLACE "${Q1590_FUNCTION_ANCHOR}"
       "${Q1590_HELPERS}${Q1590_FUNCTION_ANCHOR}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Q15.7's selector upload sits after Q10.0 has already sent uAmbientColor and
# uSunlightColor for the object. Insert immediately after that selector upload:
# Q15.9 overwrites only those two static uniforms, then pins the broad Q15.7
# encoded-domain branch off for statics.
set(Q1590_UPLOAD_ANCHOR [=[
    if (gLegacyColourDomainLocationQ1570 >= 0) {
        glUniform1f(gLegacyColourDomainLocationQ1570,
                    GetFo3LegacyColourDomainQ1570() ? 1.0f : 0.0f);
    }
]=])
set(Q1590_UPLOAD_NEW [=[
    if (gLegacyColourDomainLocationQ1570 >= 0) {
        glUniform1f(gLegacyColourDomainLocationQ1570,
                    GetFo3LegacyColourDomainQ1570() ? 1.0f : 0.0f);
    }
    Q1590UploadPcSp17LightConstants();
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1590_UPLOAD_ANCHOR}" Q1590_UPLOAD_POS)
if(Q1590_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q15.9 could not find final Q15.7 static selector upload")
endif()
string(REPLACE "${Q1590_UPLOAD_ANCHOR}" "${Q1590_UPLOAD_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Hard guards: prove the correction is scoped to the final static TU and that no
# LAND source was modified here.
string(FIND "${Q6H_NATIVE_SOURCE}" "Q1590UploadPcSp17LightConstants();" Q1590_CALL_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "gRuntime.weather.encoded[3][tod][c]" Q1590_AMBIENT_RAW_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "gRuntime.weather.encoded[4][tod][c]" Q1590_SUN_RAW_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "gRuntime.baseImage.hdrSunlightDimmer" Q1590_BASE_DIMMER_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "glUniform1f(gLegacyColourDomainLocationQ1570, 0.0f);" Q1590_LEGACY_PIN_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "Q1590" Q1590_TERRAIN_CHANGED)
if(Q1590_CALL_OK EQUAL -1 OR Q1590_AMBIENT_RAW_OK EQUAL -1 OR
   Q1590_SUN_RAW_OK EQUAL -1 OR Q1590_BASE_DIMMER_OK EQUAL -1 OR
   Q1590_LEGACY_PIN_OK EQUAL -1 OR NOT Q1590_TERRAIN_CHANGED EQUAL -1)
    message(FATAL_ERROR
        "Q15.9 verification failed: call=${Q1590_CALL_OK} ambient=${Q1590_AMBIENT_RAW_OK} sun=${Q1590_SUN_RAW_OK} baseDimmer=${Q1590_BASE_DIMMER_OK} legacyPin=${Q1590_LEGACY_PIN_OK} terrain=${Q1590_TERRAIN_CHANGED}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q15.9 PC-captured SP17 AmbientColor/PSLightColor constants enabled for static PPLighting only")
