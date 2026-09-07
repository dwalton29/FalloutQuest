# Q10.8/Q10.9: diagnose and repair the remaining per-shape NoLighting materials
# without changing geometry, movement, collision, or Bethesda-authored transforms.
#
# Device logs proved every recurring fallback building surface carries
# BSShaderNoLightingProperty, but Q10.2 rejected that property when its authored
# File Name SizedString was empty. FO3 permits a NoLighting property to exist
# without a texture; the geometry can still be driven by its authored vertex
# colour/material/alpha state. Accept the field itself, not non-empty content.

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
            Q6H_LOGW("Q10.9 MATERIAL STATE: model=%s shapeBlock=%u shapeType=%s data=%u dataType=%s properties=%s vertices=%zu triangles=%zu vertexColors=%zu noLighting=%d textureEmpty=%d shaderFlags1=%08X shaderFlags2=%08X",
                     resolved.c_str(), block, type.c_str(), shape.dataRef,
                     q1080DataType.c_str(), q1080Properties.c_str(),
                     candidate.positions.size() / 3u, candidate.indices.size() / 3u,
                     candidate.vertexColors.size() / 4u,
                     candidate.noLighting ? 1 : 0,
                     candidate.diffuseTexturePath.empty() ? 1 : 0,
                     candidate.shaderFlags1, candidate.shaderFlags2);
        }
]=])
string(REPLACE "${Q1080_OLD_FALLBACK}" "${Q1080_NEW_FALLBACK}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

# Q10.9: an empty File Name is not a parse failure for BSShaderNoLightingProperty.
set(Q1090_OLD_NOLIGHT_RETURN [=[
    return c.SizedString(fileName) && !fileName.empty();
]=])
set(Q1090_NEW_NOLIGHT_RETURN [=[
    if (!c.SizedString(fileName)) return false;
    return true;
]=])
string(REPLACE "${Q1090_OLD_NOLIGHT_RETURN}" "${Q1090_NEW_NOLIGHT_RETURN}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

# Runtime coverage marker.
string(REPLACE
    "Q10.6 NIF COVERAGE: model=%s shapeBlocks=%zu rendered=%zu shapeObjectReject=%zu shapeLoadReject=%zu fallbackDiffuse=%zu"
    "Q10.9 NIF COVERAGE: model=%s shapeBlocks=%zu rendered=%zu shapeObjectReject=%zu shapeLoadReject=%zu fallbackDiffuse=%zu"
    Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

string(FIND "${Q6H_NIF_SOURCE}" "Q10.9 MATERIAL STATE" Q1090_AUDIT_OK)
string(FIND "${Q6H_NIF_SOURCE}" "return c.SizedString(fileName) && !fileName.empty();" Q1090_BAD_REJECT)
string(FIND "${Q6H_NIF_SOURCE}" "if (!c.SizedString(fileName)) return false;" Q1090_PARSE_OK)
string(FIND "${Q6H_NIF_SOURCE}" "effectivePropertiesQ1070" Q1090_BAD_INHERITANCE)
if(Q1090_AUDIT_OK LESS 0 OR Q1090_PARSE_OK LESS 0)
    message(FATAL_ERROR "Q10.9 NoLighting material repair hook drifted: audit=${Q1090_AUDIT_OK} parse=${Q1090_PARSE_OK}")
endif()
if(NOT Q1090_BAD_REJECT LESS 0)
    message(FATAL_ERROR "Q10.9 old empty-filename rejection is still active")
endif()
if(NOT Q1090_BAD_INHERITANCE LESS 0)
    message(FATAL_ERROR "Q10.9 unsafe Q10.7 parent-property inheritance is still active")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp" "${Q6H_NIF_SOURCE}")
