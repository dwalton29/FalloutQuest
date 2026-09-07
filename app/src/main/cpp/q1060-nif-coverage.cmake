# Q10.6 NIF coverage: placement-level support can hide missing sub-shapes.
# Audit every NiTriShape/NiTriStrips block in each model so walls/floors cannot
# silently disappear while the placement is still counted as rendered.

string(REPLACE
    "    for (uint32_t block = 0; block < header.numBlocks; ++block) {\n        const std::string& type = BlockType(header, block);\n        if (type != \"NiTriStrips\" && type != \"NiTriShape\") continue;\n\n        ShapeObject shape;"
    "    size_t q1060ShapeBlocks = 0u, q1060ShapeObjectFailures = 0u, q1060ShapeLoadFailures = 0u, q1060FallbackDiffuse = 0u;\n    for (uint32_t block = 0; block < header.numBlocks; ++block) {\n        const std::string& type = BlockType(header, block);\n        if (type != \"NiTriStrips\" && type != \"NiTriShape\") continue;\n        ++q1060ShapeBlocks;\n\n        ShapeObject shape;"
    Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

set(Q1060_OLD_SHAPE_PARSE [=[
        if (!ParseShapeObject(BlockData(nif, header, block), header.blockSizes[block],
                              header, shape)) continue;
]=])
set(Q1060_NEW_SHAPE_PARSE [=[
        if (!ParseShapeObject(BlockData(nif, header, block), header.blockSizes[block],
                              header, shape)) {
            ++q1060ShapeObjectFailures;
            Q6H_LOGW("Q10.6 NIF SHAPE REJECT: model=%s block=%u type=%s stage=shape-object-parse",
                     resolved.c_str(), block, type.c_str());
            continue;
        }
]=])
string(REPLACE "${Q1060_OLD_SHAPE_PARSE}" "${Q1060_NEW_SHAPE_PARSE}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

set(Q1060_OLD_SHAPE_LOAD [=[
        if (!TryLoadShape(nif, header, shape,
                          directRootChild ? &rootTransform : nullptr,
                          candidate)) continue;
        candidate.modelPath = resolved;
        outMeshes.push_back(std::move(candidate));
]=])
set(Q1060_NEW_SHAPE_LOAD [=[
        if (!TryLoadShape(nif, header, shape,
                          directRootChild ? &rootTransform : nullptr,
                          candidate)) {
            ++q1060ShapeLoadFailures;
            const std::string dataType = shape.dataRef < header.numBlocks
                ? BlockType(header, shape.dataRef) : std::string("<invalid>");
            Q6H_LOGW("Q10.6 NIF SHAPE REJECT: model=%s block=%u type=%s data=%u dataType=%s properties=%zu stage=geometry-material-load",
                     resolved.c_str(), block, type.c_str(), shape.dataRef,
                     dataType.c_str(), shape.properties.size());
            continue;
        }
        if (candidate.diffuseTexturePath.empty()) ++q1060FallbackDiffuse;
        candidate.modelPath = resolved;
        outMeshes.push_back(std::move(candidate));
]=])
string(REPLACE "${Q1060_OLD_SHAPE_LOAD}" "${Q1060_NEW_SHAPE_LOAD}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

string(REPLACE
    "    if (outMeshes.empty()) {"
    "    Q6H_LOGI(\"Q10.6 NIF COVERAGE: model=%s shapeBlocks=%zu rendered=%zu shapeObjectReject=%zu shapeLoadReject=%zu fallbackDiffuse=%zu\", resolved.c_str(), q1060ShapeBlocks, outMeshes.size(), q1060ShapeObjectFailures, q1060ShapeLoadFailures, q1060FallbackDiffuse);\n    if (outMeshes.empty()) {"
    Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

string(FIND "${Q6H_NIF_SOURCE}" "Q10.6 NIF COVERAGE" Q1060_NIF_PATCH_OK)
if(Q1060_NIF_PATCH_OK LESS 0)
    message(FATAL_ERROR "Q10.6 NIF coverage hook drifted")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp" "${Q6H_NIF_SOURCE}")
