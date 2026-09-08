# Q11.0: strict per-shape Fallout 3 material binding.
#
# The generalized Q6H loader historically contained a convenience fallback:
# when a shape's shader property did not resolve to a BSShaderTextureSet, scan
# the entire NIF and, if it contained exactly one texture set, assign that set
# to the shape. That is unsafe for Bethesda's compound statics: an unrelated
# valid texture set can then be painted onto a different sub-shape whose own
# material is NoLighting, unsupported, empty, or otherwise distinct.
#
# Q11.0 removes that cross-shape borrowing. A shape may use only the texture
# explicitly reached through its own authored property list. We also emit one
# compact material-binding line per rendered shape so device logs can correlate
# model -> shape -> NiProperty types -> final DDS without guessing.

set(Q1100_NIF_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp")
if(NOT EXISTS "${Q1100_NIF_GENERATED}")
    message(FATAL_ERROR "Q11.0 expected generated NIF source at ${Q1100_NIF_GENERATED}")
endif()
file(READ "${Q1100_NIF_GENERATED}" Q1100_NIF_SOURCE)

set(Q1100_CROSS_SHAPE_FALLBACK [==[
    if (!candidate.noLighting && (textureSetRef >= header.numBlocks || BlockType(header, textureSetRef) != "BSShaderTextureSet")) {
        uint32_t onlyTextureSet = INVALID_REF;
        for (uint32_t block = 0; block < header.numBlocks; ++block) {
            if (BlockType(header, block) == "BSShaderTextureSet") {
                if (onlyTextureSet != INVALID_REF) {
                    onlyTextureSet = INVALID_REF;
                    break;
                }
                onlyTextureSet = block;
            }
        }
        textureSetRef = onlyTextureSet;
    }
]==])
string(FIND "${Q1100_NIF_SOURCE}" "${Q1100_CROSS_SHAPE_FALLBACK}" Q1100_FALLBACK_POS)
if(Q1100_FALLBACK_POS EQUAL -1)
    message(FATAL_ERROR "Q11.0 could not find unsafe cross-shape texture fallback")
endif()
string(REPLACE "${Q1100_CROSS_SHAPE_FALLBACK}" ""
               Q1100_NIF_SOURCE "${Q1100_NIF_SOURCE}")

set(Q1100_BIND_MARKER [==[
        candidate.modelPath = resolved;
        outMeshes.push_back(std::move(candidate));
]==])
string(FIND "${Q1100_NIF_SOURCE}" "${Q1100_BIND_MARKER}" Q1100_BIND_POS)
if(Q1100_BIND_POS EQUAL -1)
    message(FATAL_ERROR "Q11.0 could not find rendered-shape material binding point")
endif()

set(Q1100_BIND_REPLACEMENT [==[
        candidate.modelPath = resolved;
        std::string q1100Properties;
        for (uint32_t q1100Ref : shape.properties) {
            if (!q1100Properties.empty()) q1100Properties += ",";
            q1100Properties += std::to_string(q1100Ref);
            q1100Properties += ":";
            if (q1100Ref < header.numBlocks) q1100Properties += BlockType(header, q1100Ref);
            else q1100Properties += "<invalid>";
        }
        if (q1100Properties.empty()) q1100Properties = "<none>";
        Q6H_LOGI("Q11.0 MATERIAL BIND: model=%s shapeBlock=%u shapeType=%s properties=%s diffuse=%s normal=%s glow=%s noLighting=%d shaderFlags1=%08X shaderFlags2=%08X",
                 resolved.c_str(), block, type.c_str(), q1100Properties.c_str(),
                 candidate.diffuseTexturePath.empty() ? "<none>" : candidate.diffuseTexturePath.c_str(),
                 candidate.normalTexturePath.empty() ? "<none>" : candidate.normalTexturePath.c_str(),
                 candidate.glowTexturePath.empty() ? "<none>" : candidate.glowTexturePath.c_str(),
                 candidate.noLighting ? 1 : 0,
                 candidate.shaderFlags1, candidate.shaderFlags2);
        outMeshes.push_back(std::move(candidate));
]==])
string(REPLACE "${Q1100_BIND_MARKER}" "${Q1100_BIND_REPLACEMENT}"
               Q1100_NIF_SOURCE "${Q1100_NIF_SOURCE}")

# Configuration-time regression checks: never silently reintroduce the old
# whole-NIF texture-set heuristic, and require the runtime binding marker.
string(FIND "${Q1100_NIF_SOURCE}" "uint32_t onlyTextureSet = INVALID_REF;" Q1100_OLD_FALLBACK_LEFT)
string(FIND "${Q1100_NIF_SOURCE}" "Q11.0 MATERIAL BIND" Q1100_BIND_LOG_OK)
if(NOT Q1100_OLD_FALLBACK_LEFT EQUAL -1 OR Q1100_BIND_LOG_OK EQUAL -1)
    message(FATAL_ERROR "Q11.0 material binding verification failed: fallback=${Q1100_OLD_FALLBACK_LEFT} log=${Q1100_BIND_LOG_OK}")
endif()

file(WRITE "${Q1100_NIF_GENERATED}" "${Q1100_NIF_SOURCE}")
message(STATUS "Q11.0 strict authored per-shape material binding enabled")
