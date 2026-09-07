# Q10.4: robust first-entry Megaton gate discovery.
# The first Q10.0 locator assumed the XTEL destination REFR was grouped directly
# under CELL 00002DBD. Fallout's grouping does not guarantee that. Identify the
# gate by what the authored transition actually does: Capital Wasteland ->
# Megaton worldspace, then trust the destination REFR's resolved owner.

set(Q1040_GATE_LOCATOR [=[
bool QueueFo3MegatonEntryQ1040() {
    constexpr uint32_t WASTELAND_WORLDSPACE = 0x0000003Cu;
    constexpr uint32_t MEGATON_WORLDSPACE = 0x00000A74u;
    constexpr uint32_t PREFERRED_ENTRANCE_CELL = 0x00002DBDu;

    FILE* file = std::fopen(ESM_PATH, "rb");
    if (!file) {
        Q71_LOGE("Q10.4 GATE LOCATOR FAILED: reason=open-esm");
        return false;
    }
    const int64_t fileSize = FileSize(file);
    if (fileSize < static_cast<int64_t>(HEADER_SIZE)) {
        std::fclose(file);
        Q71_LOGE("Q10.4 GATE LOCATOR FAILED: reason=bad-esm-size");
        return false;
    }

    struct GateCandidateQ1040 {
        uint32_t sourceRef = 0u;
        uint32_t sourceCell = 0u;
        uint32_t sourceWorld = 0u;
        uint32_t destinationRef = 0u;
        uint32_t destinationCell = 0u;
        uint32_t destinationWorld = 0u;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        float rx = 0.0f, ry = 0.0f, rz = 0.0f;
        uint32_t flags = 0u;
        int score = -1;
    } best;

    std::vector<GroupFrame> groups;
    size_t exteriorRefs = 0u;
    size_t wastelandXtel = 0u;
    size_t resolvedDestinations = 0u;
    size_t megatonTransitions = 0u;

    while (true) {
        const off_t rawOffset = ftello(file);
        if (rawOffset < 0) break;
        const uint64_t offset = static_cast<uint64_t>(rawOffset);
        while (!groups.empty() && offset >= groups.back().end) groups.pop_back();
        if (offset + HEADER_SIZE > static_cast<uint64_t>(fileSize)) break;

        uint8_t header[HEADER_SIZE]{};
        if (!ReadExact(file, header, sizeof(header))) break;
        const uint32_t sizeField = ReadLe32(header + 4u);
        if (std::memcmp(header, "GRUP", 4u) == 0) {
            if (sizeField < HEADER_SIZE || offset + sizeField > static_cast<uint64_t>(fileSize)) break;
            groups.push_back(GroupFrame{offset + sizeField,
                                        ReadLe32(header + 8u),
                                        ReadLe32(header + 12u)});
            continue;
        }

        const uint32_t recordFlags = ReadLe32(header + 8u);
        const uint32_t sourceRef = ReadLe32(header + 12u);
        const uint64_t payloadEnd = offset + HEADER_SIZE + sizeField;
        if (payloadEnd > static_cast<uint64_t>(fileSize)) break;
        if (std::memcmp(header, "REFR", 4u) != 0) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }

        uint32_t sourceWorld = 0u;
        uint32_t sourceCell = 0u;
        for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
            if (sourceCell == 0u &&
                (it->type == 6u || it->type == 8u || it->type == 9u || it->type == 10u)) {
                sourceCell = it->label;
            }
            if (sourceWorld == 0u && it->type == 1u) sourceWorld = it->label;
        }
        if (sourceWorld != WASTELAND_WORLDSPACE) {
            if (fseeko(file, static_cast<off_t>(payloadEnd), SEEK_SET) != 0) break;
            continue;
        }
        ++exteriorRefs;

        std::vector<uint8_t> payload;
        if (!ReadPayload(file, sizeField, recordFlags, payload)) break;
        WalkSubrecords(payload.data(), payload.size(),
                       [&](const char* type, const uint8_t* bytes, uint32_t size) {
            if (std::memcmp(type, "XTEL", 4u) != 0 || size < 28u) return;
            ++wastelandXtel;

            const uint32_t destinationRef = ReadLe32(bytes + 0u);
            Q74Owner destinationOwner;
            if (!Q74ResolveOwner(destinationRef, destinationOwner) || !destinationOwner.valid) return;
            ++resolvedDestinations;

            const bool exactEntranceCell = destinationOwner.cellFormId == PREFERRED_ENTRANCE_CELL;
            const bool megatonWorld = destinationOwner.worldspaceFormId == MEGATON_WORLDSPACE;
            if (!megatonWorld && !exactEntranceCell) return;
            ++megatonTransitions;

            int score = megatonWorld ? 100 : 50;
            if (exactEntranceCell) score += 100;
            if (score <= best.score) return;

            best.sourceRef = sourceRef;
            best.sourceCell = sourceCell;
            best.sourceWorld = sourceWorld;
            best.destinationRef = destinationRef;
            best.destinationCell = destinationOwner.cellFormId != 0u
                ? destinationOwner.cellFormId : PREFERRED_ENTRANCE_CELL;
            best.destinationWorld = MEGATON_WORLDSPACE;
            best.x = ReadLeFloat(bytes + 4u);
            best.y = ReadLeFloat(bytes + 8u);
            best.z = ReadLeFloat(bytes + 12u);
            best.rx = ReadLeFloat(bytes + 16u);
            best.ry = ReadLeFloat(bytes + 20u);
            best.rz = ReadLeFloat(bytes + 24u);
            best.flags = size >= 32u ? ReadLe32(bytes + 28u) : 0u;
            best.score = score;
        });
    }
    std::fclose(file);

    Q71_LOGI("Q10.4 GATE SCAN: wastelandRefs=%zu wastelandXTEL=%zu resolvedDest=%zu toMegaton=%zu",
             exteriorRefs, wastelandXtel, resolvedDestinations, megatonTransitions);

    if (best.score < 0 || best.destinationRef == 0u || best.destinationCell == 0u) {
        Q71_LOGE("Q10.4 GATE LOCATOR FAILED: reason=no-wasteland-to-megaton-transition");
        return false;
    }

    gPendingTransitionQ74 = {};
    gPendingTransitionQ74.destinationDoorRef = best.destinationRef;
    gPendingTransitionQ74.cellFormId = best.destinationCell;
    gPendingTransitionQ74.worldspaceFormId = best.destinationWorld;
    gPendingTransitionQ74.x = best.x;
    gPendingTransitionQ74.y = best.y;
    gPendingTransitionQ74.z = best.z;
    gPendingTransitionQ74.rx = best.rx;
    gPendingTransitionQ74.ry = best.ry;
    gPendingTransitionQ74.rz = best.rz;
    gPendingTransitionQ74.valid = true;
    gHasPendingTransitionQ74 = true;

    Q71_LOGI("Q10.4 GATE READY: sourceDoor=%08X sourceCell=%08X sourceWorld=%08X destinationDoor=%08X destinationCell=%08X destinationWorld=%08X XTEL=(%.2f %.2f %.2f) R=(%.4f %.4f %.4f) score=%d source=Fallout3.esm",
             best.sourceRef, best.sourceCell, best.sourceWorld,
             best.destinationRef, best.destinationCell, best.destinationWorld,
             best.x, best.y, best.z, best.rx, best.ry, best.rz,
             best.score);
    return true;
}

]=])
string(REPLACE
    "bool ConsumeFo3CellTransitionRequestQ74(Fo3CellTransitionRequestQ74& outRequest) {"
    "${Q1040_GATE_LOCATOR}bool ConsumeFo3CellTransitionRequestQ74(Fo3CellTransitionRequestQ74& outRequest) {"
    Q720_CELL_SOURCE_TEXT "${Q720_CELL_SOURCE_TEXT}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-cell-spawn-q720.cpp"
     "${Q720_CELL_SOURCE_TEXT}")

# Renderer-side direct boot now calls the robust locator. Keep the old Q10.0
# locator compiled for diagnostics/back-compat, but do not use it for startup.
string(REPLACE
    "bool QueueFo3MegatonEntryQ1000();"
    "bool QueueFo3MegatonEntryQ1000();\nbool QueueFo3MegatonEntryQ1040();"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "if (!QueueFo3MegatonEntryQ1000()) {"
    "if (!QueueFo3MegatonEntryQ1040()) {"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "Q10.3 DIRECT BOOT BEGIN: targetCell=00002DBD worldspace=00000A74 stage=first-render"
    "Q10.4 DIRECT BOOT BEGIN: transition=CapitalWasteland-to-Megaton worldspace=00000A74 stage=first-render"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "Q10.3 DIRECT BOOT XTEL READY: targetCell=00002DBD source=Fallout3.esm"
    "Q10.4 DIRECT BOOT XTEL READY: destination=resolved-by-XTEL-owner source=Fallout3.esm"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "Q10.3 DIRECT MEGATON ENTRY READY: cell=00002DBD worldspace=00000A74 objects=%zu sceneReady=%d bootstrapCell=NONE source=Fallout3.esm/XTEL"
    "Q10.4 DIRECT MEGATON ENTRY READY: destination=resolved-by-XTEL worldspace=00000A74 objects=%zu sceneReady=%d bootstrapCell=NONE source=Fallout3.esm/XTEL"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
