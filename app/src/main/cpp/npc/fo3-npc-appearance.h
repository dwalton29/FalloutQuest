#pragma once
#include "fo3-npc.h"
#include "fo3-actor-animation.h"
#include <algorithm>
#include <cctype>

namespace fo3appearance {
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
            (item.bipedMask & 3u) && SameModel(item.modelPath, model)) return true;
    return false;
}
inline bool SkinMaterial(uint32_t type, uint32_t flags) {
    // Fallout 3 SHADER_SKIN / F3SF1_FaceGen, not Skyrim's tint flags.
    return type == 14u || (flags & 0x00000400u) != 0u;
}
inline bool HeadBindTransform(const fo3anim::Skeleton& skeleton,
                              const fo3anim::Matrix& placement,
                              fo3anim::Matrix& gameTransform) {
    const int head = fo3anim::FindBone(skeleton, "Bip01 Head");
    fo3anim::Matrix inverse;
    if (head < 0 || static_cast<size_t>(head) >= skeleton.bindGlobal.size() ||
        !fo3anim::Inverse(placement, inverse)) return false;
    gameTransform = fo3anim::Multiply(placement,
        fo3anim::Multiply(skeleton.bindGlobal[head], inverse));
    return true;
}
} // namespace fo3appearance
