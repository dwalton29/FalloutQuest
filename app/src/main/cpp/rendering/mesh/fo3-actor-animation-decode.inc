// Shares the existing bounds-checked Fallout 3 NIF header and block reader.
// Layouts: niftools/nifxml nif.xml (20.2.0.7, user 11, Bethesda 34).
#include "fo3-actor-animation.h"
#include <functional>
namespace {
bool ActorHeader(const std::vector<uint8_t> &bytes, NifHeader &h) {
  if (bytes.size() > MAX_NIF_BYTES || !ParseHeader(bytes, h))
    return false;
  const size_t end = h.blockOffsets.back() + h.blockSizes.back();
  Cursor footer(bytes.data() + end, bytes.size() - end);
  uint32_t count = 0;
  if (!footer.U32(count) || count == 0 || count > h.numBlocks)
    return false;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t root = 0;
    if (!footer.U32(root) || root >= h.numBlocks)
      return false;
  }
  return footer.remaining() == 0;
}
bool ActorValid(float v) { return std::isfinite(v) && std::fabs(v) < 1e30f; }
bool ActorReadTransform(Cursor &c, fo3anim::Track &t) {
  for (float &v : t.base.translation)
    if (!c.F32(v))
      return false;
  for (float &v : t.base.rotation)
    if (!c.F32(v))
      return false;
  if (!c.F32(t.base.scale))
    return false;
  t.hasTranslation = std::all_of(t.base.translation.begin(),
                                 t.base.translation.end(), ActorValid);
  t.hasRotation =
      std::all_of(t.base.rotation.begin(), t.base.rotation.end(), ActorValid);
  t.hasScale = ActorValid(t.base.scale) && t.base.scale > 0;
  return true;
}
bool ActorKeys(Cursor &c, fo3anim::Channel &channel, int dimensions,
               bool quat = false,
               std::array<fo3anim::Channel, 3> *axes = nullptr) {
  channel.dimensions = dimensions;
  uint32_t count = 0;
  if (!c.U32(count) || count > 100000)
    return false;
  if (!count)
    return true;
  if (!c.U32(channel.interpolation))
    return false;
  // TBC channels need their own evaluator; reject instead of silently
  // substituting a different interpolation. Supplied idle/walk use 1 and 2.
  if (quat && channel.interpolation == 4 && axes) {
    for (auto &axis : *axes)
      if (!ActorKeys(c, axis, 1))
        return false;
    return true;
  }
  if (channel.interpolation != 1 && channel.interpolation != 2)
    return false;
  if (quat && channel.interpolation != 1)
    return false;
  channel.keys.resize(count);
  float last = -1e30f;
  for (auto &key : channel.keys) {
    if (!c.F32(key.time) || !ActorValid(key.time) || key.time < last)
      return false;
    last = key.time;
    for (int i = 0; i < dimensions; ++i)
      if (!c.F32(key.value[i]) || !ActorValid(key.value[i]))
        return false;
    if (channel.interpolation == 2 && !quat) {
      for (int i = 0; i < dimensions; ++i)
        if (!c.F32(key.forward[i]) || !ActorValid(key.forward[i]))
          return false;
      for (int i = 0; i < dimensions; ++i)
        if (!c.F32(key.backward[i]) || !ActorValid(key.backward[i]))
          return false;
    }
  }
  return true;
}
bool ActorSplineChannel(const std::vector<uint8_t> &bytes, const NifHeader &h,
                        uint32_t data, uint32_t basis, uint32_t handle,
                        int dimensions, bool compact, float offset, float range,
                        fo3anim::Channel &channel) {
  channel.dimensions = dimensions;
  if (handle == 0xffffu)
    return true;
  if (data >= h.numBlocks || basis >= h.numBlocks ||
      BlockType(h, data) != "NiBSplineData" ||
      BlockType(h, basis) != "NiBSplineBasisData")
    return false;
  Cursor bc(BlockData(bytes, h, basis), h.blockSizes[basis]);
  uint32_t count = 0;
  if (!bc.U32(count) || count < 4 || count > 100000)
    return false;
  Cursor dc(BlockData(bytes, h, data), h.blockSizes[data]);
  uint32_t floats = 0, shorts = 0;
  if (!dc.U32(floats) || floats > dc.remaining() / 4)
    return false;
  size_t floatStart = dc.offset();
  if (!dc.Skip(static_cast<size_t>(floats) * 4) || !dc.U32(shorts) ||
      shorts > dc.remaining() / 2)
    return false;
  size_t shortStart = dc.offset();
  uint64_t end =
      static_cast<uint64_t>(handle) + static_cast<uint64_t>(count) * dimensions;
  if (end > (compact ? shorts : floats))
    return false;
  if (compact && (!ActorValid(offset) || !ActorValid(range)))
    return false;
  const uint8_t *payload = BlockData(bytes, h, data);
  channel.controls.resize(static_cast<size_t>(count) * dimensions);
  for (size_t i = 0; i < channel.controls.size(); ++i) {
    float value =
        compact ? offset + range *
                               static_cast<float>(static_cast<int16_t>(ReadLe16(
                                   payload + shortStart + (handle + i) * 2))) /
                               32767.0f
                : ReadLeFloat(payload + floatStart + (handle + i) * 4);
    if (!ActorValid(value))
      return false;
    channel.controls[i] = value;
  }
  return true;
}
bool ActorTrack(const std::vector<uint8_t> &bytes, const NifHeader &h,
                uint32_t block, fo3anim::Track &t) {
  if (block >= h.numBlocks)
    return false;
  const std::string &type = BlockType(h, block);
  Cursor c(BlockData(bytes, h, block), h.blockSizes[block]);
  if (type == "NiTransformInterpolator") {
    uint32_t data = INVALID_REF;
    if (!ActorReadTransform(c, t) || !c.U32(data))
      return false;
    if (data == INVALID_REF)
      return true;
    if (data >= h.numBlocks || BlockType(h, data) != "NiTransformData")
      return false;
    Cursor keys(BlockData(bytes, h, data), h.blockSizes[data]);
    if (!ActorKeys(keys, t.rotation, 4, true, &t.xyzRotation) ||
        !ActorKeys(keys, t.translation, 3) || !ActorKeys(keys, t.scale, 1))
      return false;
    t.hasRotation |= !t.rotation.keys.empty() || t.rotation.interpolation == 4;
    t.hasTranslation |= !t.translation.keys.empty();
    t.hasScale |= !t.scale.keys.empty();
    return keys.remaining() == 0;
  }
  if (type != "NiBSplineCompTransformInterpolator" &&
      type != "NiBSplineTransformInterpolator")
    return false;
  uint32_t data = 0, basis = 0, handles[3]{};
  if (!c.F32(t.start) || !c.F32(t.stop) || !ActorValid(t.start) ||
      !ActorValid(t.stop) || t.stop < t.start || !c.U32(data) ||
      !c.U32(basis) || !ActorReadTransform(c, t))
    return false;
  for (auto &handle : handles)
    if (!c.U32(handle))
      return false;
  bool compact = type == "NiBSplineCompTransformInterpolator";
  float offsets[3]{}, ranges[3]{};
  if (compact)
    for (int i = 0; i < 3; ++i)
      if (!c.F32(offsets[i]) || !c.F32(ranges[i]))
        return false;
  if (!ActorSplineChannel(bytes, h, data, basis, handles[0], 3, compact,
                          offsets[0], ranges[0], t.translation) ||
      !ActorSplineChannel(bytes, h, data, basis, handles[1], 4, compact,
                          offsets[1], ranges[1], t.rotation) ||
      !ActorSplineChannel(bytes, h, data, basis, handles[2], 1, compact,
                          offsets[2], ranges[2], t.scale))
    return false;
  t.hasTranslation |= !t.translation.controls.empty();
  t.hasRotation |= !t.rotation.controls.empty();
  t.hasScale |= !t.scale.controls.empty();
  return c.remaining() == 0;
}
} // namespace

static bool DecodeHierarchy(const std::vector<uint8_t> &bytes, fo3anim::Skeleton &out,
                            std::vector<int> *blockBones = nullptr) {
  using namespace fo3anim;
  out = {};
  Skeleton result;
  NifHeader h;
  if (!ActorHeader(bytes, h) || h.userVersion != 11 || h.bsVersion != 34)
    return false;
  struct Node {
    uint32_t block = 0;
    Bone bone;
    std::vector<uint32_t> children;
  };
  std::vector<Node> nodes;
  std::unordered_map<uint32_t, size_t> index;
  for (uint32_t block = 0; block < h.numBlocks; ++block) {
    const auto &type = BlockType(h, block);
    if (type != "NiNode" && type != "BSFadeNode" &&
        !(blockBones && Q970IsShapeType(type)))
      continue;
    Cursor name(BlockData(bytes, h, block), h.blockSizes[block]);
    uint32_t ni = 0;
    if (!name.U32(ni) || ni >= h.strings.size())
      return false;
    Cursor c(BlockData(bytes, h, block), h.blockSizes[block]);
    NifTransform transform;
    if (!ParseAvObjectPrefix(c, h, transform))
      return false;
    Node node;
    node.block = block;
    node.bone.name = h.strings[ni];
    for (int i = 0; i < 3; ++i)
      node.bone.bind.translation[i] = transform.translation[i];
    node.bone.bind.rotation = QuaternionFromMatrix(transform.rotation);
    node.bone.bind.scale = transform.scale;
    for (float v : transform.translation)
      if (!ActorValid(v))
        return false;
    for (float v : transform.rotation)
      if (!ActorValid(v))
        return false;
    if (!ActorValid(transform.scale) || transform.scale <= 0)
      return false;
    uint32_t children = 0;
    if (!Q970IsShapeType(type) && (!c.U32(children) || children > h.numBlocks))
      return false;
    node.children.resize(children);
    for (auto &ref : node.children)
      if (!c.U32(ref))
        return false;
    index[block] = nodes.size();
    nodes.push_back(std::move(node));
  }
  if (nodes.empty())
    return false;
  std::vector<int> parents(nodes.size(), -1);
  for (size_t i = 0; i < nodes.size(); ++i)
    for (uint32_t child : nodes[i].children) {
      auto it = index.find(child);
      if (it == index.end())
        continue;
      if (parents[it->second] != -1)
        return false;
      parents[it->second] = static_cast<int>(i);
    }
  if (blockBones) blockBones->assign(h.numBlocks, -1);
  std::vector<uint8_t> state(nodes.size());
  std::function<bool(size_t, int)> visit = [&](size_t i, int parent) {
    if (state[i])
      return false;
    state[i] = 1;
    int dst = static_cast<int>(result.bones.size());
    if (blockBones) (*blockBones)[nodes[i].block] = dst;
    nodes[i].bone.parent = parent;
    result.bones.push_back(nodes[i].bone);
    for (uint32_t child : nodes[i].children) {
      auto it = index.find(child);
      if (it != index.end() && !visit(it->second, dst))
        return false;
    }
    return true;
  };
  for (size_t i = 0; i < nodes.size(); ++i)
    if (parents[i] < 0 && !visit(i, -1)) {
      out = {};
      return false;
    }
  if (result.bones.size() != nodes.size() || !FinalizeSkeleton(result)) {
    out = {};
    return false;
  }
  out = std::move(result);
  return true;
}

bool fo3anim::DecodeSkeleton(const std::vector<uint8_t> &bytes, Skeleton &out) {
  return DecodeHierarchy(bytes, out);
}
bool fo3anim::DecodeModelHierarchy(const std::vector<uint8_t> &bytes, Skeleton &out,
                                  std::vector<int> &blockBones) {
  blockBones.clear();
  const bool ok = DecodeHierarchy(bytes, out, &blockBones);
  if (!ok) blockBones.clear();
  return ok;
}

static bool DecodeClips(const std::vector<uint8_t> &bytes, std::vector<fo3anim::Clip> &out) {
  using namespace fo3anim;
  out.clear();
  NifHeader h;
  if (!ActorHeader(bytes, h) || h.userVersion != 11 ||
      (h.bsVersion < 24 || h.bsVersion > 34))
    return false;
  for (uint32_t block = 0; block < h.numBlocks; ++block) {
    if (BlockType(h, block) != "NiControllerSequence")
      continue;
    Clip clip;
    Cursor c(BlockData(bytes, h, block), h.blockSizes[block]);
    uint32_t name = 0, count = 0, grow = 0;
    if (!c.U32(name) || name >= h.strings.size() || !c.U32(count) ||
        count > 4096 || !c.U32(grow))
      return false;
    clip.name = h.strings[name];
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t interp = 0, controller = 0, strings[5]{};
      uint8_t priority = 0;
      if (!c.U32(interp) || !c.U32(controller) || !c.U8(priority))
        return false;
      for (uint32_t &str : strings)
        if (!c.U32(str))
          return false;
      if (strings[0] >= h.strings.size())
        return false;
      if (strings[2] >= h.strings.size() ||
          h.strings[strings[2]] != "NiTransformController") {
        ++clip.ignoredControllers;
        continue;
      }
      Track track;
      track.bone = h.strings[strings[0]];
      if (!ActorTrack(bytes, h, interp, track)) {
        Q6H_LOGW("ACTOR CLIP UNSUPPORTED: block=%u bone=%s type=%s", interp,
                 track.bone.c_str(), BlockType(h, interp).c_str());
        return false;
      }
      if (BlockType(h, interp) == "NiBSplineCompTransformInterpolator")
        ++clip.compressedTracks;
      clip.tracks.push_back(std::move(track));
    }
    float weight = 0;
    uint32_t text = 0, manager = 0, root = 0;
    uint16_t notes = 0;
    if (!c.F32(weight) || !c.U32(text) || !c.U32(clip.cycle) ||
        clip.cycle > 2 || !c.F32(clip.frequency) || !c.F32(clip.start) ||
        !c.F32(clip.stop) || !c.U32(manager) || !c.U32(root) ||
        !ActorValid(clip.start) || !ActorValid(clip.stop) ||
        clip.stop < clip.start || !ActorValid(clip.frequency) ||
        clip.frequency <= 0)
      return false;
    // Vanilla FO3 talking/conversation idles include BS versions 25 and 27: nifxml defines a
    // single BSAnimNotes reference for 24..28, versus a ushort array after 28.
    if(h.bsVersion>=24&&h.bsVersion<=28) {
      uint32_t note=INVALID_REF;
      if(!c.U32(note)||(note!=INVALID_REF&&(note>=h.numBlocks||BlockType(h,note)!="BSAnimNotes")))return false;
    } else if(!c.U16(notes)||!c.Skip(static_cast<size_t>(notes)*4))return false;
    if(c.remaining())return false;
    if (root < h.strings.size())
      clip.accumulationRoot = h.strings[root];
    if (text != INVALID_REF) {
      if (text >= h.numBlocks || BlockType(h, text) != "NiTextKeyExtraData")
        return false;
      Cursor tc(BlockData(bytes, h, text), h.blockSizes[text]);
      uint32_t extraName = 0, keys = 0;
      if (!tc.U32(extraName) || !tc.U32(keys) || keys > 4096) return false;
      float previous = -1e30f;
      for (uint32_t i = 0; i < keys; ++i) {
        TextKey key; uint32_t value = 0;
        if (!tc.F32(key.time) || !ActorValid(key.time) || key.time < previous ||
            !tc.U32(value) || value >= h.strings.size()) return false;
        previous = key.time; key.text = h.strings[value];
        clip.textKeys.push_back(std::move(key));
      }
      if (tc.remaining()) return false;
    }
    if (clip.tracks.empty()) return false;
    out.push_back(std::move(clip));
  }
  return !out.empty();
}

bool fo3anim::DecodeClip(const std::vector<uint8_t> &bytes, Clip &out) {
  out = {};
  std::vector<Clip> clips;
  if (!DecodeClips(bytes, clips)) return false;
  out = std::move(clips.front());
  return true;
}

bool fo3anim::DecodeUiAnimation(const std::vector<uint8_t> &bytes, Skeleton &out,
                               std::vector<int> &blockBones, std::vector<Clip> &clips) {
  out = {}; blockBones.clear(); clips.clear();
  Skeleton hierarchy; std::vector<int> mapping; std::vector<Clip> sequences;
  if (!DecodeHierarchy(bytes, hierarchy, &mapping) || !DecodeClips(bytes, sequences))
    return false;
  for (auto &clip : sequences) {
    // UI root transforms are part of the authored canvas, not actor root motion.
    clip.accumulationRoot.clear();
    if (clip.ignoredControllers) return false;
    for (const auto &track : clip.tracks)
      if (FindBone(hierarchy, track.bone) < 0) return false;
  }
  out = std::move(hierarchy); blockBones = std::move(mapping); clips = std::move(sequences);
  return true;
}

#include "fo3-furniture.h"
bool fo3furniture::DecodeMarkers(const std::vector<uint8_t>& bytes,std::vector<Marker>& out) {
  out.clear();NifHeader h;if(!ActorHeader(bytes,h)||h.userVersion!=11||h.bsVersion!=34)return false;
  std::vector<Marker> decoded;
  for(uint32_t block=0;block<h.numBlocks;++block) {
    if(BlockType(h,block)!="BSFurnitureMarker")continue;
    Cursor c(BlockData(bytes,h,block),h.blockSizes[block]);uint32_t name=0,count=0;
    if(!c.U32(name)||name>=h.strings.size()||h.strings[name]!="FRN"||!c.U32(count)||count>30||c.remaining()!=size_t(count)*16)return false;
    if(!decoded.empty())return false; // Ambiguous duplicated FRN blocks.
    for(uint32_t i=0;i<count;++i){Marker m;for(auto& v:m.position)if(!c.F32(v)||!std::isfinite(v))return false;
      if(!c.U16(m.orientation)||!c.U8(m.reference)||!c.U8(m.reference2)||m.reference!=m.reference2)return false;
      decoded.push_back(m);
    }
  }
  if(decoded.empty())return false;
  out=std::move(decoded);return true;
}
