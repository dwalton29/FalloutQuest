# Q16.0 active-renderer door metadata bridge.
#
# The historical q7-runtime.cmake added DOOR/XTL metadata and AABBs to an older
# generated renderer, but that transform is no longer in the live Q6H pipeline.
# Q16 needs only the useful part of it: retain record type + VR-space bounds on
# the actual rendered objects, resolve XTEL once per authored DOOR from the ESM,
# and provide the same robust 3 m ray/AABB primitive to the final renderer.

# -----------------------------------------------------------------------------
# Transition TU: expose a generic source-REFR -> XTEL resolver. This translation
# unit already contains the proven ESM payload/subrecord helpers from
# fo3-cell-spawn.cpp and Q74ResolveOwner, so do not duplicate an ESM parser in
# the renderer.
# -----------------------------------------------------------------------------
if(NOT EXISTS "${Q720_CELL_SOURCE}")
    message(FATAL_ERROR "Q16.0 door metadata expected ${Q720_CELL_SOURCE}")
endif()
file(READ "${Q720_CELL_SOURCE}" Q1698_CELL_SOURCE)
string(PREPEND Q1698_CELL_SOURCE "#include \"fo3-cell-traversal-q1700.h\"\n")

set(Q1698_XTEL_RESOLVER [==[
bool ResolveFo3DoorTeleportQ1700(uint32_t sourceDoorRef,
                                 Fo3DoorTeleport* outTeleport) {
    if (!outTeleport) return false;
    *outTeleport = {};
    if (sourceDoorRef == 0u) return false;

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) return false;
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        return false;
    }

    bool foundRef = false;
    bool foundXtel = false;
    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            continue;
        }

        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        const uint32_t formId = ReadLe32(header + 12u);
        if (formId != sourceDoorRef || std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        foundRef = true;
        const uint32_t recordFlags = ReadLe32(header + 8u);
        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, recordFlags, payload)) break;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (foundXtel || std::memcmp(type, "XTEL", 4u) != 0 || size < 28u) return;
            outTeleport->destinationDoorRefFormId = ReadLe32(bytes + 0u);
            outTeleport->x = ReadLeFloat(bytes + 4u);
            outTeleport->y = ReadLeFloat(bytes + 8u);
            outTeleport->z = ReadLeFloat(bytes + 12u);
            outTeleport->rx = ReadLeFloat(bytes + 16u);
            outTeleport->ry = ReadLeFloat(bytes + 20u);
            outTeleport->rz = ReadLeFloat(bytes + 24u);
            if (size >= 32u) outTeleport->flags = ReadLe32(bytes + 28u);
            foundXtel = outTeleport->destinationDoorRefFormId != 0u;
        });
        break;
    }
    std::fclose(file);

    if (!foundXtel) {
        if (foundRef) {
            Q71_LOGI("Q16.0 DOOR XTEL ABSENT: sourceDoor=%08X", sourceDoorRef);
        }
        *outTeleport = {};
        return false;
    }

    Q74Owner owner;
    if (Q74ResolveOwner(outTeleport->destinationDoorRefFormId, owner) && owner.valid) {
        outTeleport->destinationCellFormId = owner.cellFormId;
    }
    outTeleport->valid = true;
    Q71_LOGI("Q16.0 DOOR XTEL READY: sourceDoor=%08X destinationDoor=%08X destinationCell=%08X XTEL=(%.2f %.2f %.2f)",
             sourceDoorRef, outTeleport->destinationDoorRefFormId,
             outTeleport->destinationCellFormId,
             outTeleport->x, outTeleport->y, outTeleport->z);
    return true;
}

]==])
set(Q1698_CELL_MARKER [==[
bool ConsumeFo3CellTransitionRequestQ74(Fo3CellTransitionRequestQ74& outRequest) {
]==])
string(FIND "${Q1698_CELL_SOURCE}" "${Q1698_CELL_MARKER}" Q1698_CELL_MARKER_POS)
if(Q1698_CELL_MARKER_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 door metadata could not find Q7.4 consume marker")
endif()
string(REPLACE "${Q1698_CELL_MARKER}"
       "${Q1698_XTEL_RESOLVER}${Q1698_CELL_MARKER}"
       Q1698_CELL_SOURCE "${Q1698_CELL_SOURCE}")
file(WRITE "${Q720_CELL_SOURCE}" "${Q1698_CELL_SOURCE}")

# -----------------------------------------------------------------------------
# Active Q6H renderer: retain record type, cached XTEL and rendered AABB.
# -----------------------------------------------------------------------------
set(Q1698_GPU_FIELDS_OLD [==[
    uint32_t refFormId = 0;
    uint32_t baseFormId = 0;
    std::string editorId;
    std::string modelPath;
};
]==])
set(Q1698_GPU_FIELDS_NEW [==[
    uint32_t refFormId = 0;
    uint32_t baseFormId = 0;
    std::string editorId;
    std::string modelPath;
    std::string baseRecordType;
    Fo3DoorTeleport teleport;
    float minX = 0.0f, maxX = 0.0f;
    float minY = 0.0f, maxY = 0.0f;
    float minZ = 0.0f, maxZ = 0.0f;
};
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1698_GPU_FIELDS_OLD}" Q1698_GPU_FIELDS_POS)
if(Q1698_GPU_FIELDS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 door metadata could not find active GpuObject tail")
endif()
string(REPLACE "${Q1698_GPU_FIELDS_OLD}" "${Q1698_GPU_FIELDS_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# The old mutable-Q7 renderer owned this symbol; current Q10+ boots Megaton's
# persistent exterior CELL directly, so initialize it to that live CELL.
string(REPLACE
    "bool gLoggedFirstDraw = false;"
    "bool gLoggedFirstDraw = false;\nuint32_t gCurrentCellFormId = 0x00002DBDu;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1698_BOUNDS_OLD [==[
    std::vector<float> expanded;
    expanded.reserve(cpu.mesh.indices.size() * FLOATS_PER_VERTEX);

    for (uint32_t index : cpu.mesh.indices) {
]==])
set(Q1698_BOUNDS_NEW [==[
    std::vector<float> expanded;
    expanded.reserve(cpu.mesh.indices.size() * FLOATS_PER_VERTEX);
    Vec3 objectMinimum{1e30f, 1e30f, 1e30f};
    Vec3 objectMaximum{-1e30f, -1e30f, -1e30f};

    for (uint32_t index : cpu.mesh.indices) {
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1698_BOUNDS_OLD}" Q1698_BOUNDS_POS)
if(Q1698_BOUNDS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 door metadata could not find UploadCpuObject vertex buffer")
endif()
string(REPLACE "${Q1698_BOUNDS_OLD}" "${Q1698_BOUNDS_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1698_POINT_OLD [==[
        const Vec3 n = GameDirectionToOpenXr(cpu.normalsGame[index]);
]==])
set(Q1698_POINT_NEW [==[
        objectMinimum.x = std::min(objectMinimum.x, p.x);
        objectMinimum.y = std::min(objectMinimum.y, p.y);
        objectMinimum.z = std::min(objectMinimum.z, p.z);
        objectMaximum.x = std::max(objectMaximum.x, p.x);
        objectMaximum.y = std::max(objectMaximum.y, p.y);
        objectMaximum.z = std::max(objectMaximum.z, p.z);
        const Vec3 n = GameDirectionToOpenXr(cpu.normalsGame[index]);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1698_POINT_OLD}" Q1698_POINT_POS)
if(Q1698_POINT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 door metadata could not find mapped vertex point")
endif()
string(REPLACE "${Q1698_POINT_OLD}" "${Q1698_POINT_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1698_CACHE_HELPER [==[
bool ResolveDoorTeleportCachedQ1698(uint32_t refFormId, Fo3DoorTeleport& out) {
    static std::unordered_map<uint32_t, Fo3DoorTeleport> cache;
    const auto cached = cache.find(refFormId);
    if (cached != cache.end()) {
        out = cached->second;
        return out.valid;
    }
    Fo3DoorTeleport resolved;
    ResolveFo3DoorTeleportQ1700(refFormId, &resolved);
    cache.emplace(refFormId, resolved);
    out = resolved;
    return out.valid;
}

]==])
set(Q1698_UPLOAD_MARKER "bool UploadCpuObject(CpuObject& cpu, float centerX, float centerY, float floorZ,")
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1698_UPLOAD_MARKER}" Q1698_UPLOAD_POS)
if(Q1698_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 door metadata could not find UploadCpuObject entry")
endif()
string(REPLACE "${Q1698_UPLOAD_MARKER}"
       "${Q1698_CACHE_HELPER}${Q1698_UPLOAD_MARKER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1698_META_OLD [==[
    gpu.editorId = cpu.placement.editorId;
    gpu.modelPath = cpu.placement.modelPath;
]==])
set(Q1698_META_NEW [==[
    gpu.editorId = cpu.placement.editorId;
    gpu.modelPath = cpu.placement.modelPath;
    gpu.baseRecordType = cpu.placement.baseRecordType;
    if (gpu.baseRecordType == "DOOR") {
        ResolveDoorTeleportCachedQ1698(gpu.refFormId, gpu.teleport);
    }
    gpu.minX = objectMinimum.x; gpu.maxX = objectMaximum.x;
    gpu.minY = objectMinimum.y; gpu.maxY = objectMaximum.y;
    gpu.minZ = objectMinimum.z; gpu.maxZ = objectMaximum.z;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1698_META_OLD}" Q1698_META_POS)
if(Q1698_META_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 door metadata could not find GPU metadata assignment")
endif()
string(REPLACE "${Q1698_META_OLD}" "${Q1698_META_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Q1700 was originally authored against q7-runtime's RayAabbQ7 and insertion
# marker. Supply those two tiny compatibility surfaces from the active renderer;
# the legacy activation function routes into Q16 after Q1700 inserts its helper.
set(Q1698_RAY_AND_MARKER [==[
bool RayAabbQ7(float ox, float oy, float oz,
               float dx, float dy, float dz,
               const GpuObject& object, float& outT) {
    constexpr float PAD = 0.08f;
    const float mins[3]{object.minX - PAD, object.minY - PAD, object.minZ - PAD};
    const float maxs[3]{object.maxX + PAD, object.maxY + PAD, object.maxZ + PAD};
    const float origins[3]{ox, oy, oz};
    const float dirs[3]{dx, dy, dz};
    float tMin = 0.0f;
    float tMax = 3.0f;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(dirs[axis]) < 1e-6f) {
            if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) return false;
            continue;
        }
        float t1 = (mins[axis] - origins[axis]) / dirs[axis];
        float t2 = (maxs[axis] - origins[axis]) / dirs[axis];
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return false;
    }
    outT = tMin;
    return tMax >= 0.0f && tMin <= 3.0f;
}

bool ActivateDoorInternalQ7(float ox, float oy, float oz,
                            float dx, float dy, float dz) {
    return ActivateDoorInternalQ1700(ox, oy, oz, dx, dy, dz);
}

]==])
set(Q1698_DRAW_MARKER "void DrawSceneObject(const GpuObject& object) {")
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1698_DRAW_MARKER}" Q1698_DRAW_POS)
if(Q1698_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q16.0 door metadata could not find DrawSceneObject insertion point")
endif()
string(REPLACE "${Q1698_DRAW_MARKER}"
       "${Q1698_RAY_AND_MARKER}${Q1698_DRAW_MARKER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

message(STATUS "Q16.0 active DOOR metadata/AABB/XTEL cache wired into Q6H renderer")
