#include "player/fo3-player-state.h"
#include "world/interaction/fo3-interaction-ray.h"
#include "world/interaction/fo3-interaction.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
using fo3player::Reference;
struct Vec3 {
  float x, y, z;
};
struct GpuObject {
  bool q220LooseObject = true;
  uint32_t refFormId = 10, baseFormId = 100;
  float minX = -.1f, maxX = .1f, minY = -.1f, maxY = .1f, minZ = -1.1f,
        maxZ = -.9f;
  float q220DynamicTransform[16]{1, 0, 0, 0, 0, 1, 0, 0,
                                 0, 0, 1, 0, 0, 0, 0, 1};
};
bool gSceneReady = true, loading = false, occluded = false, doorPresent = false;
std::unique_ptr<fo3player::Session> gPlayerSession;
std::vector<GpuObject> gObjects;
struct Grab {
  uint32_t refFormId = 0;
};
Grab gQ220Grab[2];
std::unordered_map<uint32_t, int> gQ223DynamicBodies;
std::unordered_set<uint32_t> removedCollision;
int flushes = 0, doorActivations = 0;
bool IsFo3LoadingVisible() { return loading; }
Vec3 Q220TransformPoint(const float *m, Vec3 p) {
  return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
          m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
          m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
}
bool QueryDoorInternalQ1700(float, float, float, float, float, float,
                            Fo3DoorAimQ1700 *out) {
  *out = {};
  if (!doorPresent)
    return false;
  out->valid = true;
  out->sourceDoorRef = 20;
  out->distance = 2;
  return true;
}
bool ActivateDoorInternalQ1700(float, float, float, float, float, float) {
  ++doorActivations;
  return true;
}
bool HasFo3InteractionOccluder(float, float, float, float, float, float, float,
                               uint32_t) {
  return occluded;
}
bool GetFo3DoorPromptQ1840(uint32_t, char *p, size_t n) {
  std::snprintf(p, n, "Open Door");
  return true;
}
void SetFo3CollectedCollisionRefs(const std::unordered_set<uint32_t> &r) {
  removedCollision = r;
}
void FlushFo3PlayerState() { ++flushes; }
#define Q6H_LOGI(...) ((void)0)
#include "world/interaction/fo3-interaction-runtime.inc"
void Check(bool ok, const char *why) {
  if (!ok)
    throw std::runtime_error(why);
}
int main() {
  try {
    fo3player::Catalog c;
    fo3player::Item item;
    item.formId = 100;
    item.name = "Authored Item";
    item.maxCondition = 100;
    item.weight = 2;
    c.items[100] = item;
    Reference ref;
    ref.base = 100;
    ref.count = 3;
    ref.condition = .4f;
    c.references[10] = ref;
    ref = {};
    ref.base = 200;
    c.references[20] = ref;
    gPlayerSession = std::make_unique<fo3player::Session>(c);
    gObjects.push_back({});
    doorPresent = true;
    Fo3InteractionTarget t;
    auto query = [&] { return QueryFo3Interaction(0, 0, 0, 0, 0, -2, t); };
    auto activate = [&] { return ActivateFo3Interaction(0, 0, 0, 0, 0, -1); };
    Check(query() && t.reference == 10 && t.allowed &&
              std::strcmp(t.prompt.data(), "Take Authored Item") == 0,
          "nearest item beats farther door");
    occluded = true;
    Check(!query() && !activate(),
          "collision blocker hides and prevents activation");
    occluded = false;
    loading = true;
    Check(!query() && !activate(), "loading blocks interaction");
    loading = false;
    gPlayerSession->saveBlocked = true;
    Check(query() && !t.allowed && !activate(),
          "preserved invalid save blocks pickup");
    gPlayerSession->saveBlocked = false;
    gObjects[0].q220DynamicTransform[12] = 1;
    Check(query() && !t.pickup, "moved item leaves original ray");
    gObjects[0].q220DynamicTransform[12] = 0;
    gQ220Grab[0].refFormId = 10;
    gQ223DynamicBodies[10] = 1;
    Check(!activate() && gPlayerSession->player.IsCollected(10),
          "pickup does not request VR origin reset");
    Check(flushes == 1 && removedCollision.count(10) &&
              gQ220Grab[0].refFormId == 0 && gQ223DynamicBodies.empty(),
          "pickup retires physics/grab and flushes state");
    const auto &stack = gPlayerSession->player.Snapshot().inventory.front();
    Check(stack.count == 3 && stack.condition == .4f,
          "authored quantities and condition");
    Check(query() && !t.pickup && activate() && doorActivations == 1,
          "collected item no longer targets; door queues");
    Check(gPlayerSession->player.Snapshot().inventory.front().count == 3,
          "repeat activation does not duplicate item");
    c.references[10].owner = 99;
    gPlayerSession = std::make_unique<fo3player::Session>(c);
    Check(query() && t.pickup && !t.allowed && !activate(),
          "owned item blocks action and does not activate door behind it");
    c.references[20].locked = true;
    c.references[20].key = 300;
    fo3player::Item key;
    key.kind = fo3player::ItemKind::Key;
    key.formId = 300;
    c.items[300] = key;
    gPlayerSession = std::make_unique<fo3player::Session>(c);
    gObjects.clear();
    Check(query() && !t.allowed && !activate(),
          "locked door does not teleport without key");
    Check(gPlayerSession->player.Add(300, 1) && query() && t.allowed &&
              activate(),
          "authored key permits door");
    c.scriptedBases.insert(200);
    gPlayerSession = std::make_unique<fo3player::Session>(c);
    Check(!gPlayerSession->player.CanOpenDoor(20),
          "scripted door not bypassed");
    float distance = 0;
    using namespace fo3interaction;
    Check(Box({0, 0, 0}, {0, 0, -1}, {-.1f, -.1f, -1.1f}, {.1f, .1f, -.9f}, 3,
              distance) &&
              std::fabs(distance - .9f) < 1e-5f,
          "slab distance");
    Check(!Box({NAN, 0, 0}, {0, 0, -1}, {-1, -1, -1}, {1, 1, 1}, 3, distance),
          "nonfinite ray rejected");
    Check(Triangle({0, 0, 0}, {0, 0, -1}, {-1, -1, -1}, {1, -1, -1}, {0, 1, -1},
                   2),
          "wall occludes");
    Check(!Triangle({0, 0, 0}, {0, 0, -1}, {-1, -1, -1}, {1, -1, -1},
                    {0, 1, -1}, .5f),
          "wall behind target does not occlude");
    std::cout << "Interaction runtime tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
