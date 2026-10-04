#pragma once

#include "npc/fo3-actor-animation.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace fo3dooranim {

// Immutable animation data decoded from the original Fallout 3 door NIF.
// blockBones maps each original NIF AVObject/shape block to the hierarchy used
// by fo3anim::Sample, so flattened static geometry can receive the exact
// authored node delta without re-uploading vertices.
struct Asset {
    fo3anim::Skeleton hierarchy;
    std::vector<int> blockBones;
    std::vector<fo3anim::Clip> clips;
    int openClip = -1;
    int closeClip = -1;
    // When Open/Close both drive one identical node, its delta can also move
    // the cached authored BHK at the settled endpoint.
    int collisionBone = -1;
};

inline bool EqualName(const std::string& value, const char* wanted) {
    size_t n = 0u;
    while (wanted[n]) ++n;
    if (value.size() != n) return false;
    for (size_t i = 0u; i < n; ++i) {
        const auto a = static_cast<unsigned char>(value[i]);
        const auto b = static_cast<unsigned char>(wanted[i]);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
    return true;
}

inline bool Configure(fo3anim::Skeleton hierarchy,
                      std::vector<int> blockBones,
                      std::vector<fo3anim::Clip> clips,
                      Asset& out) {
    out = {};
    int open = -1, close = -1;
    for (size_t i = 0u; i < clips.size(); ++i) {
        if (EqualName(clips[i].name, "Open"))
            open = static_cast<int>(i);
        else if (EqualName(clips[i].name, "Close"))
            close = static_cast<int>(i);
    }
    if (open < 0 || close < 0 || hierarchy.bones.empty() ||
        blockBones.empty()) return false;

    out.hierarchy = std::move(hierarchy);
    out.blockBones = std::move(blockBones);
    out.clips = std::move(clips);
    out.openClip = open;
    out.closeClip = close;

    const auto& openSequence = out.clips[static_cast<size_t>(open)];
    const auto& closeSequence = out.clips[static_cast<size_t>(close)];
    if (openSequence.tracks.size() == 1u &&
        closeSequence.tracks.size() == 1u &&
        openSequence.tracks[0].bone == closeSequence.tracks[0].bone) {
        out.collisionBone =
            fo3anim::FindBone(out.hierarchy, openSequence.tracks[0].bone);
    }
    return true;
}

inline const fo3anim::Clip* Sequence(const Asset& asset, bool opening) {
    const int index = opening ? asset.openClip : asset.closeClip;
    if (index < 0 || static_cast<size_t>(index) >= asset.clips.size())
        return nullptr;
    return &asset.clips[static_cast<size_t>(index)];
}

inline double Duration(const Asset& asset, bool opening) {
    const fo3anim::Clip* clip = Sequence(asset, opening);
    if (!clip || !(clip->frequency > 0.0f)) return 0.0;
    return std::max(0.0,
        static_cast<double>(clip->stop - clip->start) /
        static_cast<double>(clip->frequency));
}

inline int ShapeBone(const Asset& asset, uint32_t shapeBlock) {
    if (shapeBlock >= asset.blockBones.size()) return -1;
    const int bone = asset.blockBones[shapeBlock];
    return bone >= 0 &&
           static_cast<size_t>(bone) < asset.hierarchy.bones.size()
        ? bone : -1;
}

inline bool Sample(const Asset& asset, bool opening, double elapsed,
                   fo3anim::Pose& pose) {
    const fo3anim::Clip* clip = Sequence(asset, opening);
    return clip && fo3anim::Sample(asset.hierarchy, *clip, elapsed, pose);
}

inline const fo3anim::Matrix* ShapeDelta(const Asset& asset,
                                         const fo3anim::Pose& pose,
                                         int bone) {
    if (bone < 0 ||
        static_cast<size_t>(bone) >= asset.hierarchy.bones.size() ||
        static_cast<size_t>(bone) >= pose.delta.size()) return nullptr;
    return &pose.delta[static_cast<size_t>(bone)];
}

inline const fo3anim::Matrix* CollisionDelta(const Asset& asset,
                                             const fo3anim::Pose& pose) {
    return ShapeDelta(asset, pose, asset.collisionBone);
}

} // namespace fo3dooranim
