# Q10.7: preserve Fallout 3 NIF hierarchy semantics that the static loader
# previously flattened too aggressively. Bethesda node subclasses share the
# NiNode/NiTriBasedGeom common prefixes; their subclass tails are irrelevant to
# static mesh placement, but parent transforms/properties are not.

set(Q1070_OLD_PARSE_NODE [=[
bool ParseNode(const uint8_t* data, size_t size, const NifHeader& header,
               NifTransform& transform, std::vector<uint32_t>& children) {
    Cursor c(data, size);
    if (!ParseAvObjectPrefix(c, header, transform)) return false;
    uint32_t childCount = 0;
    if (!c.U32(childCount) || childCount > MAX_BLOCKS) return false;
    children.resize(childCount);
    for (uint32_t& child : children) if (!c.U32(child)) return false;
    uint32_t effectCount = 0;
    if (!c.U32(effectCount) || effectCount > MAX_BLOCKS ||
        !c.Skip(static_cast<size_t>(effectCount) * 4u)) return false;
    return c.remaining() == 0u;
}
]=])
set(Q1070_NEW_PARSE_NODE [=[
bool ParseNode(const uint8_t* data, size_t size, const NifHeader& header,
               NifTransform& transform, std::vector<uint32_t>& children,
               std::vector<uint32_t>* properties = nullptr) {
    Cursor c(data, size);
    if (!ParseAvObjectPrefix(c, header, transform, properties)) return false;
    uint32_t childCount = 0;
    if (!c.U32(childCount) || childCount > MAX_BLOCKS) return false;
    children.resize(childCount);
    for (uint32_t& child : children) if (!c.U32(child)) return false;
    uint32_t effectCount = 0;
    if (!c.U32(effectCount) || effectCount > MAX_BLOCKS ||
        !c.Skip(static_cast<size_t>(effectCount) * 4u)) return false;
    // Known NiNode subclasses append their own fields after the common NiNode
    // body. Block sizes bound the cursor, so retaining the common hierarchy is
    // safer and more faithful than rejecting the entire parent node.
    return true;
}
]=])
string(REPLACE "${Q1070_OLD_PARSE_NODE}" "${Q1070_NEW_PARSE_NODE}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

set(Q1070_OLD_PARSE_SHAPE [=[
bool ParseShapeObject(const uint8_t* data, size_t size, const NifHeader& header,
                      ShapeObject& out) {
    Cursor c(data, size);
    if (!ParseAvObjectPrefix(c, header, out.transform, &out.properties)) return false;

    uint32_t skinRef = INVALID_REF;
    if (!c.U32(out.dataRef) || !c.U32(skinRef)) return false;

    uint32_t numMaterials = 0;
    if (!c.U32(numMaterials) || numMaterials > 4096u) return false;
    if (!c.Skip(static_cast<size_t>(numMaterials) * 4u) ||
        !c.Skip(static_cast<size_t>(numMaterials) * 4u)) return false;

    uint32_t activeMaterial = 0;
    uint8_t dirtyFlag = 0;
    if (!c.U32(activeMaterial) || !c.U8(dirtyFlag)) return false;
    return c.remaining() == 0u;
}
]=])
set(Q1070_NEW_PARSE_SHAPE [=[
bool ParseShapeObject(const uint8_t* data, size_t size, const NifHeader& header,
                      ShapeObject& out) {
    Cursor c(data, size);
    if (!ParseAvObjectPrefix(c, header, out.transform, &out.properties)) return false;

    uint32_t skinRef = INVALID_REF;
    if (!c.U32(out.dataRef) || !c.U32(skinRef)) return false;

    uint32_t numMaterials = 0;
    if (!c.U32(numMaterials) || numMaterials > 4096u) return false;
    if (!c.Skip(static_cast<size_t>(numMaterials) * 4u) ||
        !c.Skip(static_cast<size_t>(numMaterials) * 4u)) return false;

    uint32_t activeMaterial = 0;
    uint8_t dirtyFlag = 0;
    if (!c.U32(activeMaterial) || !c.U8(dirtyFlag)) return false;
    // BSSegmentedTriShape / BSLODTriShape append subclass data after the
    // NiTriBasedGeom common body. We only need the common geometry/material
    // references for the static renderer, so preserve the shape instead of
    // rejecting it solely because a valid subclass tail remains.
    return true;
}

bool IsNodeLikeQ1070(const std::string& type) {
    return type == "NiNode" || type == "BSFadeNode" ||
           type == "BSOrderedNode" || type == "BSValueNode" ||
           type == "BSLeafAnimNode" || type == "BSTreeNode" ||
           type == "BSMultiBoundNode" || type == "RootCollisionNode";
}

bool IsShapeLikeQ1070(const std::string& type) {
    return type == "NiTriStrips" || type == "NiTriShape" ||
           type == "BSSegmentedTriShape" || type == "BSLODTriShape";
}
]=])
string(REPLACE "${Q1070_OLD_PARSE_SHAPE}" "${Q1070_NEW_PARSE_SHAPE}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

# Parent discovery previously ignored every Bethesda NiNode subclass and also
# discarded node-level NiProperties. Gamebryo properties propagate down the
# scene graph, so collect them root -> child and let direct shape properties win.
set(Q1070_OLD_ANCESTORS [=[
bool CollectAncestorTransforms(const std::vector<uint8_t>& nif,
                               const NifHeader& header,
                               uint32_t childBlock,
                               std::vector<NifTransform>& ancestors) {
    ancestors.clear();
    if (childBlock >= header.numBlocks) return false;

    std::vector<uint32_t> parent(header.numBlocks, INVALID_REF);
    std::vector<NifTransform> nodeTransforms(header.numBlocks);
    std::vector<uint8_t> parsedNode(header.numBlocks, 0u);

    for (uint32_t block = 0; block < header.numBlocks; ++block) {
        const std::string& type = BlockType(header, block);
        if (type != "NiNode" && type != "BSFadeNode") continue;
        std::vector<uint32_t> children;
        NifTransform transform;
        if (!ParseNode(BlockData(nif, header, block), header.blockSizes[block],
                       header, transform, children)) continue;
        nodeTransforms[block] = transform;
        parsedNode[block] = 1u;
        for (uint32_t child : children) {
            if (child < header.numBlocks && parent[child] == INVALID_REF) parent[child] = block;
        }
    }

    uint32_t current = parent[childBlock];
    for (uint32_t guard = 0; current < header.numBlocks && guard < header.numBlocks; ++guard) {
        if (!parsedNode[current]) break;
        ancestors.push_back(nodeTransforms[current]);
        const uint32_t next = parent[current];
        if (next == current) break;
        current = next;
    }
    return !ancestors.empty();
}
]=])
set(Q1070_NEW_ANCESTORS [=[
bool CollectAncestorTransforms(const std::vector<uint8_t>& nif,
                               const NifHeader& header,
                               uint32_t childBlock,
                               std::vector<NifTransform>& ancestors,
                               std::vector<uint32_t>* inheritedProperties = nullptr) {
    ancestors.clear();
    if (inheritedProperties) inheritedProperties->clear();
    if (childBlock >= header.numBlocks) return false;

    std::vector<uint32_t> parent(header.numBlocks, INVALID_REF);
    std::vector<NifTransform> nodeTransforms(header.numBlocks);
    std::vector<std::vector<uint32_t>> nodeProperties(header.numBlocks);
    std::vector<uint8_t> parsedNode(header.numBlocks, 0u);

    for (uint32_t block = 0; block < header.numBlocks; ++block) {
        const std::string& type = BlockType(header, block);
        if (!IsNodeLikeQ1070(type)) continue;
        std::vector<uint32_t> children;
        NifTransform transform;
        std::vector<uint32_t> properties;
        if (!ParseNode(BlockData(nif, header, block), header.blockSizes[block],
                       header, transform, children, &properties)) continue;
        nodeTransforms[block] = transform;
        nodeProperties[block] = std::move(properties);
        parsedNode[block] = 1u;
        for (uint32_t child : children) {
            if (child < header.numBlocks && parent[child] == INVALID_REF) parent[child] = block;
        }
    }

    std::vector<uint32_t> chain;
    uint32_t current = parent[childBlock];
    for (uint32_t guard = 0; current < header.numBlocks && guard < header.numBlocks; ++guard) {
        if (!parsedNode[current]) break;
        ancestors.push_back(nodeTransforms[current]);
        chain.push_back(current);
        const uint32_t next = parent[current];
        if (next == current) break;
        current = next;
    }
    if (inheritedProperties) {
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            const std::vector<uint32_t>& props = nodeProperties[*it];
            inheritedProperties->insert(inheritedProperties->end(), props.begin(), props.end());
        }
    }
    return !ancestors.empty();
}
]=])
string(REPLACE "${Q1070_OLD_ANCESTORS}" "${Q1070_NEW_ANCESTORS}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

string(REPLACE
    "        if (type != \"NiNode\" && type != \"BSFadeNode\") continue;"
    "        if (!IsNodeLikeQ1070(type)) continue;"
    Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")
string(REPLACE
    "        if (type != \"NiTriStrips\" && type != \"NiTriShape\") continue;"
    "        if (!IsShapeLikeQ1070(type)) continue;"
    Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

set(Q1070_OLD_PROPERTY_START [=[
    uint32_t textureSetRef = INVALID_REF;
    for (uint32_t ref : shape.properties) {
]=])
set(Q1070_NEW_PROPERTY_START [=[
    std::vector<NifTransform> ancestors;
    std::vector<uint32_t> inheritedPropertiesQ1070;
    CollectAncestorTransforms(nif, header, shape.block, ancestors, &inheritedPropertiesQ1070);
    std::vector<uint32_t> effectivePropertiesQ1070 = inheritedPropertiesQ1070;
    effectivePropertiesQ1070.insert(effectivePropertiesQ1070.end(),
                                    shape.properties.begin(), shape.properties.end());

    uint32_t textureSetRef = INVALID_REF;
    for (uint32_t ref : effectivePropertiesQ1070) {
]=])
string(REPLACE "${Q1070_OLD_PROPERTY_START}" "${Q1070_NEW_PROPERTY_START}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

set(Q1070_OLD_LATE_ANCESTORS [=[
    std::vector<NifTransform> ancestors;
    CollectAncestorTransforms(nif, header, shape.block, ancestors);
    ApplyTransforms(candidate, shape.transform, ancestors, root);
]=])
set(Q1070_NEW_LATE_ANCESTORS [=[
    ApplyTransforms(candidate, shape.transform, ancestors, root);
]=])
string(REPLACE "${Q1070_OLD_LATE_ANCESTORS}" "${Q1070_NEW_LATE_ANCESTORS}"
       Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

# Coverage line now reports the expanded geometry vocabulary and inherited
# property use so device logs can prove whether these paths matter in Megaton.
string(REPLACE
    "Q10.6 NIF COVERAGE: model=%s shapeBlocks=%zu rendered=%zu shapeObjectReject=%zu shapeLoadReject=%zu fallbackDiffuse=%zu"
    "Q10.7 NIF COVERAGE: model=%s shapeBlocks=%zu rendered=%zu shapeObjectReject=%zu shapeLoadReject=%zu fallbackDiffuse=%zu"
    Q6H_NIF_SOURCE "${Q6H_NIF_SOURCE}")

string(FIND "${Q6H_NIF_SOURCE}" "IsNodeLikeQ1070" Q1070_NODE_OK)
string(FIND "${Q6H_NIF_SOURCE}" "effectivePropertiesQ1070" Q1070_PROP_OK)
string(FIND "${Q6H_NIF_SOURCE}" "BSSegmentedTriShape" Q1070_SHAPE_OK)
if(Q1070_NODE_OK LESS 0 OR Q1070_PROP_OK LESS 0 OR Q1070_SHAPE_OK LESS 0)
    message(FATAL_ERROR "Q10.7 NIF hierarchy patch drifted: node=${Q1070_NODE_OK} prop=${Q1070_PROP_OK} shape=${Q1070_SHAPE_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp" "${Q6H_NIF_SOURCE}")
