# Q15.8: non-visual Megaton directional-light / NdotL trace.
#
# PC apitrace gives hard inputs for a representative Megaton PPLighting draw:
# AmbientColor=(41,147,182)/255, PSLightColor=2.5*(255,206,170)/255 and
# LightData=(-0.684054,0.286799,0.670684). This diagnostic changes no pixels.
# It samples the same OpenXR-space vertex normals sent to GLES and compares their
# NdotL against (a) the exact Quest sun uniform, (b) that vector passed through
# the game->OpenXR direction bridge, and (c) its inverse.

set(Q1580_GPU_FIELDS_OLD [=[
    uint32_t refFormId = 0;
    uint32_t baseFormId = 0;
    std::string editorId;
    std::string modelPath;
};
]=])
set(Q1580_GPU_FIELDS_NEW [=[
    uint32_t refFormId = 0;
    uint32_t baseFormId = 0;
    std::string editorId;
    std::string modelPath;
    std::vector<Vec3> q1580NormalSamples;
};
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1580_GPU_FIELDS_OLD}" Q1580_GPU_FIELDS_POS)
if(Q1580_GPU_FIELDS_POS EQUAL -1)
    message(FATAL_ERROR "Q15.8 could not find final GpuObject metadata anchor")
endif()
string(REPLACE "${Q1580_GPU_FIELDS_OLD}" "${Q1580_GPU_FIELDS_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1580_SAMPLE_OLD [=[
    gpu.refFormId = cpu.placement.refFormId;
    gpu.baseFormId = cpu.placement.baseFormId;
    gpu.editorId = cpu.placement.editorId;
    gpu.modelPath = cpu.placement.modelPath;
]=])
set(Q1580_SAMPLE_NEW [=[
    gpu.refFormId = cpu.placement.refFormId;
    gpu.baseFormId = cpu.placement.baseFormId;
    gpu.editorId = cpu.placement.editorId;
    gpu.modelPath = cpu.placement.modelPath;

    constexpr size_t Q1580_MAX_NORMAL_SAMPLES = 64u;
    const size_t q1580IndexCount = cpu.mesh.indices.size();
    const size_t q1580Wanted = std::min(Q1580_MAX_NORMAL_SAMPLES, q1580IndexCount);
    gpu.q1580NormalSamples.reserve(q1580Wanted);
    for (size_t q1580Sample = 0; q1580Sample < q1580Wanted; ++q1580Sample) {
        const size_t q1580StreamPos =
            (q1580Sample * q1580IndexCount) / std::max<size_t>(q1580Wanted, 1u);
        if (q1580StreamPos >= q1580IndexCount) continue;
        const uint32_t q1580Index = cpu.mesh.indices[q1580StreamPos];
        if (q1580Index >= cpu.normalsGame.size()) continue;
        gpu.q1580NormalSamples.push_back(
            GameDirectionToOpenXr(cpu.normalsGame[q1580Index]));
    }
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1580_SAMPLE_OLD}" Q1580_SAMPLE_POS)
if(Q1580_SAMPLE_POS EQUAL -1)
    message(FATAL_ERROR "Q15.8 could not find GPU metadata upload anchor")
endif()
string(REPLACE "${Q1580_SAMPLE_OLD}" "${Q1580_SAMPLE_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1580_TRACE_CODE [=[
struct Q1580NdotLStats {
    size_t samples = 0u;
    size_t zero = 0u;
    size_t below10 = 0u;
    size_t below25 = 0u;
    double sum = 0.0;
    float maximum = 0.0f;
};

void Q1580AddNdotL(Q1580NdotLStats& stats, const Vec3& normal, const Vec3& light) {
    const float ndotl = std::max(normal.x * light.x + normal.y * light.y + normal.z * light.z, 0.0f);
    ++stats.samples;
    if (ndotl <= 0.0001f) ++stats.zero;
    if (ndotl < 0.10f) ++stats.below10;
    if (ndotl < 0.25f) ++stats.below25;
    stats.sum += ndotl;
    stats.maximum = std::max(stats.maximum, ndotl);
}

float Q1580Mean(const Q1580NdotLStats& stats) {
    return stats.samples ? static_cast<float>(stats.sum / static_cast<double>(stats.samples)) : 0.0f;
}
float Q1580Pct(size_t part, size_t whole) {
    return whole ? 100.0f * static_cast<float>(part) / static_cast<float>(whole) : 0.0f;
}
bool Q1580MegatonArchitecture(const GpuObject& object) {
    return object.modelPath.find("megaton") != std::string::npos ||
           object.modelPath.find("Megaton") != std::string::npos;
}

void Q1580LogLightTrace(const Fo3EnvironmentQ1000& env) {
    static bool q1580Logged = false;
    if (q1580Logged || !env.valid || env.worldspaceFormId != 0x00000A74u ||
        !fo3todq1400::gRuntime.ready || gObjects.empty()) return;

    const Vec3 uploaded = Normalize({env.sunDirection[0], env.sunDirection[1], env.sunDirection[2]});
    const Vec3 converted = GameDirectionToOpenXr({env.sunDirection[0], env.sunDirection[1], env.sunDirection[2]});
    const Vec3 inverted{-uploaded.x, -uploaded.y, -uploaded.z};

    Q1580NdotLStats allA, allB, allInv, archA, archB, archInv;
    size_t litObjects = 0u, archObjects = 0u, details = 0u;
    for (const GpuObject& object : gObjects) {
        if (object.noLighting || object.q1580NormalSamples.empty()) continue;
        ++litObjects;
        const bool arch = Q1580MegatonArchitecture(object);
        if (arch) ++archObjects;
        Q1580NdotLStats objA, objB, objInv;
        for (const Vec3& normal : object.q1580NormalSamples) {
            Q1580AddNdotL(allA, normal, uploaded);
            Q1580AddNdotL(allB, normal, converted);
            Q1580AddNdotL(allInv, normal, inverted);
            Q1580AddNdotL(objA, normal, uploaded);
            Q1580AddNdotL(objB, normal, converted);
            Q1580AddNdotL(objInv, normal, inverted);
            if (arch) {
                Q1580AddNdotL(archA, normal, uploaded);
                Q1580AddNdotL(archB, normal, converted);
                Q1580AddNdotL(archInv, normal, inverted);
            }
        }
        if (arch && details < 12u) {
            ++details;
            Q6H_LOGI("Q15.8 LIGHT TRACE OBJECT: ref=%08X base=%08X model=%s samples=%zu uploaded(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) converted(mean=%.3f zero=%.1f%% max=%.3f) inverted(mean=%.3f zero=%.1f%% max=%.3f)",
                     object.refFormId, object.baseFormId, object.modelPath.c_str(), objA.samples,
                     Q1580Mean(objA), Q1580Pct(objA.zero,objA.samples), Q1580Pct(objA.below10,objA.samples), Q1580Pct(objA.below25,objA.samples), objA.maximum,
                     Q1580Mean(objB), Q1580Pct(objB.zero,objB.samples), objB.maximum,
                     Q1580Mean(objInv), Q1580Pct(objInv.zero,objInv.samples), objInv.maximum);
        }
    }

    Q6H_LOGI("Q15.8 LIGHT TRACE HEADER: world=%08X climate=%08X weather=%08X EDID=%s hour=%.2f uploadedSun=(%.6f %.6f %.6f) convertedCandidate=(%.6f %.6f %.6f) pcSP17LightDataUnaligned=(-0.684054 0.286799 0.670684) ambient=(%.6f %.6f %.6f) sunlight=(%.6f %.6f %.6f)",
             env.worldspaceFormId, env.climateFormId, env.weatherFormId,
             env.weatherEditorId.empty()?"<none>":env.weatherEditorId.c_str(), GetFo3TestHourQ1400(),
             uploaded.x, uploaded.y, uploaded.z, converted.x, converted.y, converted.z,
             env.ambient[0], env.ambient[1], env.ambient[2], env.sunlight[0], env.sunlight[1], env.sunlight[2]);
    Q6H_LOGI("Q15.8 LIGHT TRACE SUMMARY: litObjects=%zu samples=%zu uploaded(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) converted(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) inverted(mean=%.3f zero=%.1f%% max=%.3f)",
             litObjects, allA.samples,
             Q1580Mean(allA),Q1580Pct(allA.zero,allA.samples),Q1580Pct(allA.below10,allA.samples),Q1580Pct(allA.below25,allA.samples),allA.maximum,
             Q1580Mean(allB),Q1580Pct(allB.zero,allB.samples),Q1580Pct(allB.below10,allB.samples),Q1580Pct(allB.below25,allB.samples),allB.maximum,
             Q1580Mean(allInv),Q1580Pct(allInv.zero,allInv.samples),allInv.maximum);
    Q6H_LOGI("Q15.8 LIGHT TRACE ARCH: architectureObjects=%zu samples=%zu uploaded(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) converted(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) inverted(mean=%.3f zero=%.1f%% max=%.3f) renderChanged=0",
             archObjects, archA.samples,
             Q1580Mean(archA),Q1580Pct(archA.zero,archA.samples),Q1580Pct(archA.below10,archA.samples),Q1580Pct(archA.below25,archA.samples),archA.maximum,
             Q1580Mean(archB),Q1580Pct(archB.zero,archB.samples),Q1580Pct(archB.below10,archB.samples),Q1580Pct(archB.below25,archB.samples),archB.maximum,
             Q1580Mean(archInv),Q1580Pct(archInv.zero,archInv.samples),archInv.maximum);
    q1580Logged = true;
}

]=])

set(Q1580_FUNCTION_ANCHOR [=[
void DrawSceneObject(const GpuObject& object) {
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1580_FUNCTION_ANCHOR}" Q1580_FUNCTION_POS)
if(Q1580_FUNCTION_POS EQUAL -1)
    message(FATAL_ERROR "Q15.8 could not find DrawSceneObject insertion point")
endif()
string(REPLACE "${Q1580_FUNCTION_ANCHOR}" "${Q1580_TRACE_CODE}${Q1580_FUNCTION_ANCHOR}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Q15.7's raw-weather refresh is a stable final RenderScene anchor: it is inside
# the live static uniform upload, with q1000Env in scope, and runs before draws.
# Q1580LogLightTrace is one-shot and read-only, so calling it here changes no GL state.
set(Q1580_CALL_ANCHOR [=[
        RefreshFo3RawWeatherLightingQ1480();
]=])
set(Q1580_CALL_NEW [=[
        RefreshFo3RawWeatherLightingQ1480();
        Q1580LogLightTrace(q1000Env);
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1580_CALL_ANCHOR}" Q1580_CALL_POS)
if(Q1580_CALL_POS EQUAL -1)
    message(FATAL_ERROR "Q15.8 could not find Q15.7 final static uniform-upload anchor")
endif()
string(REPLACE "${Q1580_CALL_ANCHOR}" "${Q1580_CALL_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "std::vector<Vec3> q1580NormalSamples;" Q1580_FIELD_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q1580LogLightTrace(q1000Env);" Q1580_CALL_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "pcSP17LightDataUnaligned=(-0.684054 0.286799 0.670684)" Q1580_PC_REF_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "converted = GameDirectionToOpenXr" Q1580_CONVERTED_OK)
if(Q1580_FIELD_OK EQUAL -1 OR Q1580_CALL_OK EQUAL -1 OR Q1580_PC_REF_OK EQUAL -1 OR Q1580_CONVERTED_OK EQUAL -1)
    message(FATAL_ERROR "Q15.8 verification failed: field=${Q1580_FIELD_OK} call=${Q1580_CALL_OK} pcRef=${Q1580_PC_REF_OK} converted=${Q1580_CONVERTED_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q15.8 nonvisual Megaton light-direction/NdotL trace enabled; render output unchanged")
