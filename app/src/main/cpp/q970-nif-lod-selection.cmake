# Q9.70 makes the generated static NIF renderer follow the authored scene graph.
# Fallout 3 NIFs can contain NiSwitchNode/NiLODNode branches where only one
# child is meant to render. The old generalized loader enumerated every
# NiTriShape/NiTriStrips block, so distant/alternate geometry could appear at
# the same time as the real nearby mesh. For VR correctness we force NiLODNode
# to child 0 (the nearest/high-detail branch), respect NiSwitchNode's active
# index, and suppress hidden AVObject subtrees.

set(Q970_NIF_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/fo3-static-nif-q6h.cpp")
if(NOT EXISTS "${Q970_NIF_GENERATED}")
    message(FATAL_ERROR "Q9.70 expected generated NIF source at ${Q970_NIF_GENERATED}")
endif()
file(READ "${Q970_NIF_GENERATED}" Q970_NIF_SOURCE)

set(Q970_TRY_SHAPE_MARKER [==[
bool TryLoadShape(const std::vector<uint8_t>& nif, const NifHeader& header,
                  const ShapeObject& shape,
                  const NifTransform* root,
                  Fo3StaticNifMesh& mesh) {
]==])
string(FIND "${Q970_NIF_SOURCE}" "${Q970_TRY_SHAPE_MARKER}" Q970_TRY_SHAPE_POS)
if(Q970_TRY_SHAPE_POS EQUAL -1)
    message(FATAL_ERROR "Q9.70 could not find TryLoadShape insertion point")
endif()

set(Q970_SCENEGRAPH_HELPERS [==[
struct Q970NodeInfo {
    bool parsed = false;
    bool isSwitch = false;
    bool isLod = false;
    uint16_t flagsLow = 0u;
    uint32_t switchIndex = 0u;
    NifTransform transform;
    std::vector<uint32_t> children;
};

struct Q970SceneStats {
    size_t parsedNodes = 0u;
    size_t lodNodes = 0u;
    size_t switchNodes = 0u;
    size_t hiddenNodes = 0u;
    size_t hiddenShapes = 0u;
    size_t suppressedLodChildren = 0u;
    size_t selectedShapes = 0u;
    size_t unparentedShapes = 0u;
    bool fallbackAll = false;
};

bool Q970IsShapeType(const std::string& type) {
    return type == "NiTriStrips" || type == "NiTriShape";
}

bool Q970LooksLikeNode(const std::string& type) {
    if (type == "NiNode" || type == "BSFadeNode" ||
        type == "NiSwitchNode" || type == "NiLODNode") return true;
    return type.size() >= 4u && type.compare(type.size() - 4u, 4u, "Node") == 0;
}

bool Q970ParseAvObjectBase(Cursor& c, const NifHeader& header,
                           NifTransform& transform, uint16_t& flagsLow) {
    if (!ParseObjectNetPrefix(c)) return false;

    if (!c.U16(flagsLow)) return false;
    if (header.userVersion >= 11u && header.bsVersion > 26u) {
        uint16_t flagsHigh = 0u;
        if (!c.U16(flagsHigh)) return false;
    }

    for (float& value : transform.translation) if (!c.F32(value)) return false;
    for (float& value : transform.rotation) if (!c.F32(value)) return false;
    if (!c.F32(transform.scale)) return false;

    if (header.userVersion <= 11u) {
        uint32_t propertyCount = 0u;
        if (!c.U32(propertyCount) || propertyCount > MAX_BLOCKS) return false;
        if (!c.Skip(static_cast<size_t>(propertyCount) * 4u)) return false;
    }

    uint32_t collisionRef = INVALID_REF;
    if (!c.U32(collisionRef)) return false;
    transform.valid = true;
    return true;
}

bool Q970ReadAvFlags(const std::vector<uint8_t>& nif,
                     const NifHeader& header,
                     uint32_t block,
                     uint16_t& flagsLow) {
    if (block >= header.numBlocks) return false;
    Cursor c(BlockData(nif, header, block), header.blockSizes[block]);
    if (!ParseObjectNetPrefix(c)) return false;
    if (!c.U16(flagsLow)) return false;
    return true;
}

bool Q970ParseNodeInfo(const std::vector<uint8_t>& nif,
                       const NifHeader& header,
                       uint32_t block,
                       Q970NodeInfo& out) {
    out = {};
    if (block >= header.numBlocks) return false;
    const std::string& type = BlockType(header, block);
    if (!Q970LooksLikeNode(type)) return false;

    Cursor c(BlockData(nif, header, block), header.blockSizes[block]);
    if (!Q970ParseAvObjectBase(c, header, out.transform, out.flagsLow)) return false;

    uint32_t childCount = 0u;
    if (!c.U32(childCount) || childCount > MAX_BLOCKS) return false;
    out.children.resize(childCount);
    for (uint32_t& child : out.children) if (!c.U32(child)) return false;

    uint32_t effectCount = 0u;
    if (!c.U32(effectCount) || effectCount > MAX_BLOCKS ||
        !c.Skip(static_cast<size_t>(effectCount) * 4u)) return false;

    out.isLod = type == "NiLODNode";
    out.isSwitch = out.isLod || type == "NiSwitchNode";
    if (out.isSwitch) {
        uint16_t switchFlags = 0u;
        if (!c.U16(switchFlags) || !c.U32(out.switchIndex)) return false;
        // NiLODNode appends a Ref<NiLODData> in Fallout 3. We do not need its
        // ranges because VR correctness currently forces the nearest child.
        if (out.isLod && c.remaining() >= 4u) {
            uint32_t lodDataRef = INVALID_REF;
            if (!c.U32(lodDataRef)) return false;
        }
    }

    // Derived NiNode classes may append fields after the base node payload.
    // They do not change the child list needed for reachability here.
    out.parsed = true;
    return true;
}

bool Q970CollectAncestorTransforms(const std::vector<uint8_t>& nif,
                                    const NifHeader& header,
                                    uint32_t childBlock,
                                    std::vector<NifTransform>& ancestors) {
    ancestors.clear();
    if (childBlock >= header.numBlocks) return false;

    std::vector<uint32_t> parent(header.numBlocks, INVALID_REF);
    std::vector<Q970NodeInfo> nodes(header.numBlocks);
    for (uint32_t block = 0u; block < header.numBlocks; ++block) {
        Q970NodeInfo info;
        if (!Q970ParseNodeInfo(nif, header, block, info)) continue;
        nodes[block] = std::move(info);
        for (uint32_t child : nodes[block].children) {
            if (child < header.numBlocks && parent[child] == INVALID_REF) {
                parent[child] = block;
            }
        }
    }

    uint32_t current = parent[childBlock];
    for (uint32_t guard = 0u;
         current < header.numBlocks && guard < header.numBlocks;
         ++guard) {
        if (!nodes[current].parsed) break;
        ancestors.push_back(nodes[current].transform);
        const uint32_t next = parent[current];
        if (next == current) break;
        current = next;
    }
    return !ancestors.empty();
}

bool Q970BuildRenderableSet(const std::vector<uint8_t>& nif,
                            const NifHeader& header,
                            std::vector<uint8_t>& renderable,
                            Q970SceneStats& stats) {
    renderable.assign(header.numBlocks, 0u);
    stats = {};

    std::vector<Q970NodeInfo> nodes(header.numBlocks);
    std::vector<uint32_t> parent(header.numBlocks, INVALID_REF);
    size_t shapeCount = 0u;

    for (uint32_t block = 0u; block < header.numBlocks; ++block) {
        const std::string& type = BlockType(header, block);
        if (Q970IsShapeType(type)) ++shapeCount;

        Q970NodeInfo info;
        if (!Q970ParseNodeInfo(nif, header, block, info)) continue;
        nodes[block] = std::move(info);
        ++stats.parsedNodes;
        if (nodes[block].isLod) ++stats.lodNodes;
        else if (nodes[block].isSwitch) ++stats.switchNodes;
    }

    for (uint32_t block = 0u; block < header.numBlocks; ++block) {
        if (!nodes[block].parsed) continue;
        for (uint32_t child : nodes[block].children) {
            if (child < header.numBlocks && parent[child] == INVALID_REF) {
                parent[child] = block;
            }
        }
    }

    std::vector<uint32_t> stack;
    stack.reserve(header.numBlocks);
    for (uint32_t block = 0u; block < header.numBlocks; ++block) {
        if (nodes[block].parsed && parent[block] == INVALID_REF) stack.push_back(block);
    }

    std::vector<uint8_t> visited(header.numBlocks, 0u);
    while (!stack.empty()) {
        const uint32_t block = stack.back();
        stack.pop_back();
        if (block >= header.numBlocks || visited[block]) continue;
        visited[block] = 1u;

        const std::string& type = BlockType(header, block);
        if (nodes[block].parsed) {
            if ((nodes[block].flagsLow & 0x0001u) != 0u) {
                ++stats.hiddenNodes;
                continue;
            }

            const std::vector<uint32_t>& children = nodes[block].children;
            if (nodes[block].isLod) {
                uint32_t chosen = INVALID_REF;
                for (uint32_t child : children) {
                    if (child < header.numBlocks) {
                        chosen = child;
                        break;
                    }
                }
                if (chosen != INVALID_REF) stack.push_back(chosen);
                if (children.size() > 1u) {
                    stats.suppressedLodChildren += children.size() - 1u;
                }
            } else if (nodes[block].isSwitch) {
                uint32_t chosen = INVALID_REF;
                if (nodes[block].switchIndex < children.size()) {
                    chosen = children[nodes[block].switchIndex];
                } else {
                    for (uint32_t child : children) {
                        if (child < header.numBlocks) {
                            chosen = child;
                            break;
                        }
                    }
                }
                if (chosen < header.numBlocks) stack.push_back(chosen);
            } else {
                for (uint32_t child : children) {
                    if (child < header.numBlocks) stack.push_back(child);
                }
            }
            continue;
        }

        if (Q970IsShapeType(type)) {
            uint16_t flagsLow = 0u;
            if (Q970ReadAvFlags(nif, header, block, flagsLow) &&
                (flagsLow & 0x0001u) != 0u) {
                ++stats.hiddenShapes;
                continue;
            }
            renderable[block] = 1u;
            ++stats.selectedShapes;
        }
    }

    // Preserve valid top-level geometry that is not parented by any node.
    for (uint32_t block = 0u; block < header.numBlocks; ++block) {
        if (!Q970IsShapeType(BlockType(header, block)) ||
            parent[block] != INVALID_REF || renderable[block] != 0u) continue;
        uint16_t flagsLow = 0u;
        if (Q970ReadAvFlags(nif, header, block, flagsLow) &&
            (flagsLow & 0x0001u) != 0u) {
            ++stats.hiddenShapes;
            continue;
        }
        renderable[block] = 1u;
        ++stats.selectedShapes;
        ++stats.unparentedShapes;
    }

    // If an unfamiliar node subclass prevented traversal, fail open rather
    // than making the entire model disappear. Hidden shapes remain excluded.
    if (shapeCount > 0u && stats.selectedShapes == 0u) {
        stats.fallbackAll = true;
        for (uint32_t block = 0u; block < header.numBlocks; ++block) {
            if (!Q970IsShapeType(BlockType(header, block))) continue;
            uint16_t flagsLow = 0u;
            if (Q970ReadAvFlags(nif, header, block, flagsLow) &&
                (flagsLow & 0x0001u) != 0u) continue;
            renderable[block] = 1u;
            ++stats.selectedShapes;
        }
    }

    return shapeCount > 0u;
}

]==])
string(REPLACE "${Q970_TRY_SHAPE_MARKER}"
               "${Q970_SCENEGRAPH_HELPERS}${Q970_TRY_SHAPE_MARKER}"
               Q970_NIF_SOURCE "${Q970_NIF_SOURCE}")

# Ancestor transforms must follow switch/LOD node parents too, not only exact
# NiNode/BSFadeNode blocks.
set(Q970_ANCESTOR_OLD "    CollectAncestorTransforms(nif, header, shape.block, ancestors);")
set(Q970_ANCESTOR_NEW "    Q970CollectAncestorTransforms(nif, header, shape.block, ancestors);")
string(FIND "${Q970_NIF_SOURCE}" "${Q970_ANCESTOR_OLD}" Q970_ANCESTOR_POS)
if(Q970_ANCESTOR_POS EQUAL -1)
    message(FATAL_ERROR "Q9.70 could not find ancestor-transform call")
endif()
string(REPLACE "${Q970_ANCESTOR_OLD}" "${Q970_ANCESTOR_NEW}"
               Q970_NIF_SOURCE "${Q970_NIF_SOURCE}")

# Lighting30ShaderProperty derives from BSShaderPPLightingProperty in FO3 and
# carries the same texture-set reference layout.
set(Q970_SHADER_OLD [==[        if (type == "BSShaderPPLightingProperty") {]==])
set(Q970_SHADER_NEW [==[        if (type == "BSShaderPPLightingProperty" || type == "Lighting30ShaderProperty") {]==])
string(FIND "${Q970_NIF_SOURCE}" "${Q970_SHADER_OLD}" Q970_SHADER_POS)
if(Q970_SHADER_POS EQUAL -1)
    message(FATAL_ERROR "Q9.70 could not find PPLighting material dispatch")
endif()
string(REPLACE "${Q970_SHADER_OLD}" "${Q970_SHADER_NEW}"
               Q970_NIF_SOURCE "${Q970_NIF_SOURCE}")

# Build the renderable shape mask once per model immediately after parsing its
# NIF header.
set(Q970_ROOT_MARKER [==[
    NifTransform rootTransform;
    std::vector<uint32_t> rootChildren;
]==])
string(FIND "${Q970_NIF_SOURCE}" "${Q970_ROOT_MARKER}" Q970_ROOT_POS)
if(Q970_ROOT_POS EQUAL -1)
    message(FATAL_ERROR "Q9.70 could not find multi-loader scene root")
endif()
set(Q970_ROOT_INJECT [==[
    Q970SceneStats q970Scene;
    std::vector<uint8_t> q970Renderable;
    const bool q970SelectionReady =
        Q970BuildRenderableSet(nif, header, q970Renderable, q970Scene);
    Q6H_LOGI("Q9.70 NIF SCENEGRAPH: model=%s nodes=%zu lodNodes=%zu switchNodes=%zu selectedShapes=%zu suppressedLodChildren=%zu hiddenNodes=%zu hiddenShapes=%zu unparentedShapes=%zu fallbackAll=%d forceLOD0=1",
             resolved.c_str(), q970Scene.parsedNodes, q970Scene.lodNodes,
             q970Scene.switchNodes, q970Scene.selectedShapes,
             q970Scene.suppressedLodChildren, q970Scene.hiddenNodes,
             q970Scene.hiddenShapes, q970Scene.unparentedShapes,
             q970Scene.fallbackAll ? 1 : 0);

    NifTransform rootTransform;
    std::vector<uint32_t> rootChildren;
]==])
string(REPLACE "${Q970_ROOT_MARKER}" "${Q970_ROOT_INJECT}"
               Q970_NIF_SOURCE "${Q970_NIF_SOURCE}")

set(Q970_SHAPE_LOOP_MARKER [==[
        if (type != "NiTriStrips" && type != "NiTriShape") continue;
        ++q1060ShapeBlocks;

        ShapeObject shape;
]==])
string(FIND "${Q970_NIF_SOURCE}" "${Q970_SHAPE_LOOP_MARKER}" Q970_SHAPE_LOOP_POS)
if(Q970_SHAPE_LOOP_POS EQUAL -1)
    message(FATAL_ERROR "Q9.70 could not find current Q10.9 multi-loader shape loop")
endif()
set(Q970_SHAPE_LOOP_REPLACEMENT [==[
        if (type != "NiTriStrips" && type != "NiTriShape") continue;
        ++q1060ShapeBlocks;
        if (q970SelectionReady &&
            (block >= q970Renderable.size() || q970Renderable[block] == 0u)) {
            continue;
        }

        ShapeObject shape;
]==])
string(REPLACE "${Q970_SHAPE_LOOP_MARKER}" "${Q970_SHAPE_LOOP_REPLACEMENT}"
               Q970_NIF_SOURCE "${Q970_NIF_SOURCE}")

file(WRITE "${Q970_NIF_GENERATED}" "${Q970_NIF_SOURCE}")
message(STATUS "Q9.70 patched static NIF scene graph / forced nearest LOD")