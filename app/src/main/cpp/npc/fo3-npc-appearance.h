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
inline bool BoneLocalFacePart(const Fo3NpcActorQ230& actor, const std::string& model,
                              const std::string& attachmentBone) {
    // A Prn-bearing RACE face part is exported in the Head bone frame.
    // Some mouth meshes omit Prn and are already oriented in actor axes.
    if (!SameModel(attachmentBone, "Bip01 Head")) return false;
    for (size_t slot = 2; slot < actor.raceHeadModels.size() && slot < 8; ++slot)
        if (SameModel(actor.raceHeadModels[slot], model)) return true;
    return false;
}
constexpr uint32_t PipboySlot = 0x40u;
inline bool PipboySleeveShape(const std::string& shape, bool worn) {
    if (SameModel(shape, "PipboyOn")) return worn;
    if (SameModel(shape, "PipboyOff")) return !worn;
    return true;
}
inline bool RenderHairShape(const std::string& name, uint32_t equipment) {
    if (SameModel(name, "NoHat")) return (equipment & 0x402u) == 0u;
    if (SameModel(name, "Hat")) return (equipment & 0x400u) != 0u;
    return (equipment & 2u) == 0u;
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
inline bool HairMaterial(uint32_t flags) {
    return (flags & 0x00040000u) != 0u;
}
inline float HairTintMultiplier(uint8_t color, float mask) {
    // SP17 SM3002/SM3LL002: the green vertex channel masks the HairTint
    // constant about neutral grey. The other channels are shader controls.
    return 1.0f + mask * (2.0f * static_cast<float>(color) / 255.0f - 1.0f);
}
inline std::string FaceNormalMap(std::string diffuse) {
    const auto dot = diffuse.find_last_of('.');
    if (dot != std::string::npos) diffuse.insert(dot, "_n");
    return diffuse;
}
inline bool AttachmentBindTransform(const fo3anim::Skeleton& skeleton,
                                    const fo3anim::Matrix& placement,
                                    const std::string& boneName,
                                    fo3anim::Matrix& gameTransform,
                                    bool boneLocal) {
    const int bone = fo3anim::FindBone(skeleton, boneName);
    fo3anim::Matrix inverse;
    if (bone < 0 || static_cast<size_t>(bone) >= skeleton.bindGlobal.size() ||
        !fo3anim::Inverse(placement, inverse)) return false;
    // Some face/hair meshes already encode the actor axes in their NIF root.
    auto anchor = boneLocal ? skeleton.bindGlobal[bone] : fo3anim::Identity();
    for (int axis = 0; axis < 3; ++axis)
        anchor[12 + axis] = skeleton.bindGlobal[bone][12 + axis];
    gameTransform = fo3anim::Multiply(placement,
        fo3anim::Multiply(anchor, inverse));
    return true;
}
inline bool HeadBindTransform(const fo3anim::Skeleton& skeleton,
                              const fo3anim::Matrix& placement,
                              fo3anim::Matrix& gameTransform,
                              bool boneLocal = false) {
    return AttachmentBindTransform(skeleton,placement,"Bip01 Head",gameTransform,boneLocal);
}
} // namespace fo3appearance
