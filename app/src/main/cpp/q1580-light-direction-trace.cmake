# Q15.8: non-visual Megaton directional-light / NdotL trace.
#
# The PC apitrace finally gives us hard renderer inputs for a representative
# Megaton PPLighting draw: AmbientColor=(41,147,182)/255, PSLightColor at the
# effective 2.5x warm sunlight scale, and LightData=(-0.684054,0.286799,0.670684).
# Q15.8 changes no pixels. It preserves a tiny sample of the exact OpenXR-space
# vertex normals submitted for each static shape, then reports how strongly the
# currently-uploaded Quest sun vector illuminates those normals. A second set of
# statistics applies GameDirectionToOpenXr to the same sun vector ONLY as a
# diagnostic candidate, so an axis/space mismatch is visible in logcat without
# another subjective colour A/B test.

# -----------------------------------------------------------------------------
# Keep at most 64 representative *rendered-index* normals per GPU shape. This is
# intentionally tiny and follows the same GameDirectionToOpenXr transform used
# by the vertex buffer, so the CPU diagnostic measures the shader's input space.
# -----------------------------------------------------------------------------
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

    // Sample the expanded draw stream rather than unique source vertices. That
    // weights repeated triangle vertices the same way rasterized geometry does.
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

# -----------------------------------------------------------------------------
# CPU-side statistics. The first vector is EXACTLY q1000Env.sunDirection, i.e.
# the value uploaded to uSunDirection. The converted candidate is diagnostic
# only and never reaches GLES. The PC vector is printed as an unaligned SP17
# reference; we do not pretend its basis is proven equivalent yet.
# -----------------------------------------------------------------------------
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
    const float ndotl = std::max(normal.x * light.x +
                                 normal.y * light.y +
                                 normal.z * light.z, 0.0f);
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
    return whole ? (100.0f * static_cast<float>(part) / static_cast<float>(whole)) : 0.0f;
}

bool Q1580MegatonArchitecture(const GpuObject& object) {
    std::string path = object.modelPath;
    for (char& ch : path) {
        if (ch == '/') ch = '\\';
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return path.find("architecture\\megaton\\") != std::string::npos;
}

void Q1580LogLightTrace(const Fo3EnvironmentQ1000& env) {
    static bool q1580Logged = false;
    if (q1580Logged || !env.valid || env.worldspaceFormId != 0x00000A74u ||
        !fo3todq1400::gRuntime.ready || gObjects.empty()) return;

    const Vec3 uploaded = Normalize({
        env.sunDirection[0], env.sunDirection[1], env.sunDirection[2]});
    const Vec3 convertedCandidate = GameDirectionToOpenXr({
        env.sunDirection[0], env.sunDirection[1], env.sunDirection[2]});
    const Vec3 inverted{-uploaded.x, -uploaded.y, -uploaded.z};

    Q1580NdotLStats allUploaded, allConverted, allInverted;
    Q1580NdotLStats archUploaded, archConverted, archInverted;
    size_t litObjects = 0u;
    size_t architectureObjects = 0u;
    size_t detailedObjects = 0u;

    for (const GpuObject& object : gObjects) {
        if (object.noLighting || object.q1580NormalSamples.empty()) continue;
        ++litObjects;
        const bool architecture = Q1580MegatonArchitecture(object);
        if (architecture) ++architectureObjects;

        Q1580NdotLStats objectUploaded, objectConverted, objectInverted;
        for (const Vec3& normal : object.q1580NormalSamples) {
            Q1580AddNdotL(allUploaded, normal, uploaded);
            Q1580AddNdotL(allConverted, normal, convertedCandidate);
            Q1580AddNdotL(allInverted, normal, inverted);
            Q1580AddNdotL(objectUploaded, normal, uploaded);
            Q1580AddNdotL(objectConverted, normal, convertedCandidate);
            Q1580AddNdotL(objectInverted, normal, inverted);
            if (architecture) {
                Q1580AddNdotL(archUploaded, normal, uploaded);
                Q1580AddNdotL(archConverted, normal, convertedCandidate);
                Q1580AddNdotL(archInverted, normal, inverted);
            }
        }

        if (architecture && detailedObjects < 12u) {
            ++detailedObjects;
            Q6H_LOGI(
                "Q15.8 LIGHT TRACE OBJECT: ref=%08X base=%08X model=%s samples=%zu uploaded(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) convertedCandidate(mean=%.3f zero=%.1f%% max=%.3f) inverted(mean=%.3f zero=%.1f%% max=%.3f)",
                object.refFormId, object.baseFormId, object.modelPath.c_str(),
                objectUploaded.samples,
                Q1580Mean(objectUploaded), Q1580Pct(objectUploaded.zero, objectUploaded.samples),
                Q1580Pct(objectUploaded.below10, objectUploaded.samples),
                Q1580Pct(objectUploaded.below25, objectUploaded.samples), objectUploaded.maximum,
                Q1580Mean(objectConverted), Q1580Pct(objectConverted.zero, objectConverted.samples),
                objectConverted.maximum,
                Q1580Mean(objectInverted), Q1580Pct(objectInverted.zero, objectInverted.samples),
                objectInverted.maximum);
        }
    }

    Q6H_LOGI(
        "Q15.8 LIGHT TRACE HEADER: world=%08X climate=%08X weather=%08X EDID=%s hour=%.2f uploadedSun=(%.6f %.6f %.6f) convertedCandidate=(%.6f %.6f %.6f) pcSP17LightDataUnaligned=(-0.684054 0.286799 0.670684) ambient=(%.6f %.6f %.6f) sunlight=(%.6f %.6f %.6f)",
        env.worldspaceFormId, env.climateFormId, env.weatherFormId,
        env.weatherEditorId.empty() ? "<none>" : env.weatherEditorId.c_str(),
        GetFo3TestHourQ1400(),
        uploaded.x, uploaded.y, uploaded.z,
        convertedCandidate.x, convertedCandidate.y, convertedCandidate.z,
        env.ambient[0], env.ambient[1], env.ambient[2],
        env.sunlight[0], env.sunlight[1], env.sunlight[2]);

    Q6H_LOGI(
        "Q15.8 LIGHT TRACE SUMMARY: litObjects=%zu samples=%zu uploaded(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) convertedCandidate(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) inverted(mean=%.3f zero=%.1f%% max=%.3f)",
        litObjects, allUploaded.samples,
        Q1580Mean(allUploaded), Q1580Pct(allUploaded.zero, allUploaded.samples),
        Q1580Pct(allUploaded.below10, allUploaded.samples),
        Q1580Pct(allUploaded.below25, allUploaded.samples), allUploaded.maximum,
        Q1580Mean(allConverted), Q1580Pct(allConverted.zero, allConverted.samples),
        Q1580Pct(allConverted.below10, allConverted.samples),
        Q1580Pct(allConverted.below25, allConverted.samples), allConverted.maximum,
        Q1580Mean(allInverted), Q1580Pct(allInverted.zero, allInverted.samples), allInverted.maximum);

    Q6H_LOGI(
        "Q15.8 LIGHT TRACE ARCH: architectureObjects=%zu samples=%zu uploaded(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) convertedCandidate(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) inverted(mean=%.3f zero=%.1f%% max=%.3f) renderChanged=0",
        architectureObjects, archUploaded.samples,
        Q1580Mean(archUploaded), Q1580Pct(archUploaded.zero, archUploaded.samples),
        Q1580Pct(archUploaded.below10, archUploaded.samples),
        Q1580Pct(archUploaded.below25, archUploaded.samples), archUploaded.maximum,
        Q1580Mean(archConverted), Q1580Pct(archConverted.zero, archConverted.samples),
        Q1580Pct(archConverted.below10, archConverted.samples),
        Q1580Pct(archConverted.below25, archConverted.samples), archConverted.maximum,
        Q1580Mean(archInverted), Q1580Pct(archInverted.zero, archInverted.samples), archInverted.maximum);

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
string(REPLACE "${Q1580_FUNCTION_ANCHOR}"
       "${Q1580_TRACE_CODE}${Q1580_FUNCTION_ANCHOR}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Run after all environment/light uniforms have been prepared, immediately before
# ordinary world draws. gRuntime.ready deliberately delays the trace until Q14.0
# has rebuilt from Q14.1's spatial Megaton weather on the following frame.
set(Q1580_CALL_ANCHOR [=[
    // Opaque and alpha-tested cutouts first. Alpha testing still writes depth,
]=])
set(Q1580_CALL_NEW [=[
    Q1580LogLightTrace(q1000Env);

    // Opaque and alpha-tested cutouts first. Alpha testing still writes depth,
]=])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1580_CALL_ANCHOR}" Q1580_CALL_POS)
if(Q1580_CALL_POS EQUAL -1)
    message(FATAL_ERROR "Q15.8 could not find final RenderScene draw anchor")
endif()
string(REPLACE "${Q1580_CALL_ANCHOR}" "${Q1580_CALL_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Hard guards: prove this is diagnostics-only and that both the exact shader input
# and the coordinate-converted candidate are present in the final generated TU.
string(FIND "${Q6H_NATIVE_SOURCE}" "std::vector<Vec3> q1580NormalSamples;" Q1580_FIELD_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q1580LogLightTrace(q1000Env);" Q1580_CALL_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "pcSP17LightDataUnaligned=(-0.684054 0.286799 0.670684)" Q1580_PC_REF_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "convertedCandidate = GameDirectionToOpenXr" Q1580_CONVERTED_OK)
if(Q1580_FIELD_OK EQUAL -1 OR Q1580_CALL_OK EQUAL -1 OR
   Q1580_PC_REF_OK EQUAL -1 OR Q1580_CONVERTED_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.8 verification failed: field=${Q1580_FIELD_OK} call=${Q1580_CALL_OK} pcRef=${Q1580_PC_REF_OK} converted=${Q1580_CONVERTED_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q15.8 nonvisual Megaton light-direction/NdotL trace enabled; render output unchanged")
