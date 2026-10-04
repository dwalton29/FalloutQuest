#pragma once
#include "fo3-npc.h"
#include "fo3-actor-animation.h"
#include <algorithm>
#include <cctype>

namespace fo3appearance {
// FO3 head, hair, mask, headband, hat and eyeglasses equipment slots.
constexpr uint32_t HeadEquipmentMask = 0x00000f03u;
inline bool SameModel(std::string a, std::string b) {
    auto normalize = [](std::string& path) {
        for (char& c : path) c = c == '\\' ? '/' :
            static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };
    normalize(a); normalize(b);
    return !a.empty() && a == b;
}
inline bool HeadPart(const Fo3NpcActorQ230& actor, const std::string& model) {
    if (SameModel(actor.hairModel, model)) return true;
    for (const auto& path : actor.raceHeadModels)
        if (SameModel(path, model)) return true;
    for (const auto& path : actor.headPartModels)
        if (SameModel(path, model)) return true;
    for (const auto& item : actor.inventory)
        if (item.recordType == "ARMO" && item.count > 0 &&
            (item.bipedMask & HeadEquipmentMask) && SameModel(item.modelPath, model)) return true;
    return false;
}
inline bool BoneLocalFacePart(const Fo3NpcActorQ230& actor, const std::string& model) {
    // RACE slots 2..7: mouth, lower/upper teeth, tongue and eyes. Their
    // authored root transforms express geometry in the Head bone frame.
    for (size_t slot = 2; slot < actor.raceHeadModels.size() && slot < 8; ++slot)
        if (SameModel(actor.raceHeadModels[slot], model)) return true;
    return false;
}
inline bool RenderHairShape(const std::string& name, uint32_t equipment) {
    if (SameModel(name, "NoHat")) return (equipment & 0x400u) == 0u;
    if (SameModel(name, "Hat")) return (equipment & 0x400u) != 0u;
    return true;
}
inline std::string HairMorphModel(std::string model, const std::string& shape) {
    const auto dot = model.find_last_of('.');
    if (dot != std::string::npos &&
        (SameModel(shape, "Hat") || SameModel(shape, "NoHat")))
        model.insert(dot, SameModel(shape, "Hat") ? "hat" : "nohat");
    return model;
}
inline bool SkinMaterial(uint32_t type, uint32_t flags) {
    // Fallout 3 SHADER_SKIN / F3SF1_FaceGen, not Skyrim's tint flags.
    return type == 14u || (flags & 0x00000400u) != 0u;
}
inline bool HeadBindTransform(const fo3anim::Skeleton& skeleton,
                              const fo3anim::Matrix& placement,
                              fo3anim::Matrix& gameTransform,
                              bool boneLocal = false) {
    const int head = fo3anim::FindBone(skeleton, "Bip01 Head");
    fo3anim::Matrix inverse;
    if (head < 0 || static_cast<size_t>(head) >= skeleton.bindGlobal.size() ||
        !fo3anim::Inverse(placement, inverse)) return false;
    // FO3 rigid face/hair/headwear NIFs are already in actor axes,
    // with the head origin removed. Keep their authored orientation.
    auto anchor = boneLocal ? skeleton.bindGlobal[head] : fo3anim::Identity();
    for (int axis = 0; axis < 3; ++axis)
        anchor[12 + axis] = skeleton.bindGlobal[head][12 + axis];
    gameTransform = fo3anim::Multiply(placement,
        fo3anim::Multiply(anchor, inverse));
    return true;
}
} // namespace fo3appearance
