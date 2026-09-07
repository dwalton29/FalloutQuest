# Q10.8: diagnose the remaining per-shape fallback materials without changing
# geometry, movement, collision, or Bethesda-authored transforms.
#
# Device logs show complex Megaton buildings render every NiTriShape/NiTriStrips
# block but commonly leave exactly one shape without a diffuse texture. Print the
# direct NiProperty vocabulary on those shapes so the next implementation can
# support the actual Fallout material path instead of guessing.

set(Q1080_OLD_FALLBACK [=[
        if (candidate.diffuseTexturePath.empty()) ++q1060FallbackDiffuse;
]=])
set(Q1080_NEW_FALLBACK [=[
        if (candidate.diffuseTexturePath.empty()) {
            ++q1060FallbackDiffuse;
            std::string q1080Properties;
            for (uint32_t propertyRef : shape.properties) {
                if (!q1080Properties.empty()) q1080Properties += ",";
                if (propertyRef < header.numBlocks) q1080Properties += BlockType(header, propertyRef);
                else q1080Properties += "<invalid>";
            }
            if (q1080Properties.empty()) q1080Properties = "<none>";
            const std::string q1080DataType = shape.dataRef < header.numBlocks
                ? BlockType(header, shape.dataRef) : std::string("<invalid>");
            Q6H_LOGW("Q10.8 FALLBACK MATERIAL: model=%s shapeBlock=%u shapeType=%s data=%u dataType=%s properties=%s vertices=%zu triangles=%zu vertexColors=%zu noLighting=%d shaderFlags1=%08X shaderFlags2=%08X",
                     resolved.c_str(), block, type.c_str(), shape.dataRef,
                     q1080DataType.c_str(), q1080Properties.c_str(),
                     candidate.positions.size() / 3u, candidate.indices.size() / 3u,
                     candidate.vertexColors.size() / 4u,
                     candidate.noLighting ? 1 : 0,
                     candidate.shaderFlags1, candidate.shaderFlags2);
        }
]=])
string(REPLACE "${Q1080_OLD_FALLBACK}" "${Q1080_NEW_FALLBACK}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

# Runtime marker proving this audit was compiled after the Q10.7 regression
# rollback and before the generated loader is written.
string(REPLACE
    "Q10.6 NIF COVERAGE: model=%s shapeBlocks=%zu rendered=%zu shapeObjectReject=%zu shapeLoadReject=%zu fallbackDiffuse=%zu"
    "Q10.8 NIF COVERAGE: model=%s shapeBlocks=%zu rendered=%zu shapeObjectReject=%zu shapeLoadReject=%zu fallbackDiffuse=%zu"
    Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

string(FIND "${Q6H_NIF_SOURCE}" "Q10.8 FALLBACK MATERIAL" Q1080_AUDIT_OK)
string(FIND "${Q6H_NIF_SOURCE}" "effectivePropertiesQ1070" Q1080_BAD_INHERITANCE)
if(Q1080_AUDIT_OK LESS 0)
    message(FATAL_ERROR "Q10.8 fallback material audit hook drifted")
endif()
if(NOT Q1080_BAD_INHERITANCE LESS 0)
    message(FATAL_ERROR "Q10.8 unsafe Q10.7 parent-property inheritance is still active")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp" "${Q6H_NIF_SOURCE}")
