#include "player/fo3-player-state.h"
// Reproduce the Android platform far macro that caused the native build
// failure.
#define far
#include "world/interaction/fo3-interaction-ray.h"
#undef far
#include "world/interaction/fo3-interaction.h"
#include "world/interaction/fo3-loot-cursor.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
namespace fo3audio {
int pickups = 0, opens = 0, closes = 0, scrolls = 0;
void Pickup(uint32_t) { ++pickups; }
void Open(uint32_t) { ++opens; }
void Close(uint32_t) { ++closes; }
void Scroll() { ++scrolls; }
} // namespace fo3audio
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
    Check(fo3audio::pickups == 1,
          "successful pickup emits exactly one authored effect");
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
    c.references[20].owner = 99;
    gPlayerSession = std::make_unique<fo3player::Session>(c);
    Check(gPlayerSession->player.CanOpenDoor(20), "owned unlocked door permits entry");
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
    c.defaultActivationDoors.insert(200);
    gPlayerSession = std::make_unique<fo3player::Session>(c);
    Check(gPlayerSession->player.Add(300,1) && gPlayerSession->player.CanOpenDoor(20), "verified activation script accepts authored key");
    c.references[20].locked=false;
    gPlayerSession = std::make_unique<fo3player::Session>(c);
    Check(gPlayerSession->player.CanOpenDoor(20), "Brass Lantern passthrough permits unlocked entry");
    for (const auto kind : {fo3player::ItemKind::Ingredient,fo3player::ItemKind::Note}) {
      c.references[10].owner=0; c.items[100].kind=kind;
      gPlayerSession = std::make_unique<fo3player::Session>(c);
      gObjects.push_back({});gObjects.back().q220LooseObject=false;
      Check(query() && t.pickup && t.allowed, "catalog item targets without loose physics flag");
      gObjects.clear();
    }
    fo3player::Container box;
    box.name = "Authored Box";
    box.entries.push_back({100, 0, 4, 1, .5f, true});
    auto second = item;
    second.formId = 101;
    second.name = "Second Item";
    c.items[101] = second;
    box.entries.push_back({101, 0, 2, 1, 1, false});
    c.containers[400] = box;
    fo3player::Reference boxRef;
    boxRef.base = 400;
    c.references[40] = boxRef;
    gPlayerSession = std::make_unique<fo3player::Session>(c);
    GpuObject object;
    object.q220LooseObject = false;
    object.refFormId = 40;
    object.baseFormId = 400;
    gObjects.push_back(object);
    Check(query() && t.container && !t.pickup && t.allowed,
          "container joins nearest right-hand targets");
    const int audioOpens = fo3audio::opens, audioPickups = fo3audio::pickups,
              audioCloses = fo3audio::closes;
    UpdateFo3LootSelection(t, 0, 1);
    Check(GetFo3LootPanel().reference == 40 &&
              GetFo3LootPanel().rows.size() == 2,
          "floating rows contain authored stacks");
    Check(fo3audio::opens == audioOpens + 1, "container aim opens once");
    UpdateFo3LootSelection(t, -1, 1.1);
    Check(GetFo3LootPanel().selected == 1,
          "right stick down selects next loot row");
    const auto previousDoors = doorActivations;
    Check(!activate() && doorActivations == previousDoors &&
              GetFo3LootPanel().rows.size() == 1,
          "A takes selected stack without teleport or submenu");
    Check(!gPlayerSession->player.IsCollected(40) &&
              !removedCollision.count(40),
          "container stays in world after looting");
    Check(fo3audio::pickups == audioPickups + 1 &&
              fo3audio::opens == audioOpens + 1 && fo3audio::scrolls == 1,
          "loot transfer and selection sound without reopening");
    UpdateFo3LootSelection({}, 0, 2);
    Check(fo3audio::closes == audioCloses + 1, "aim loss closes once");
    Check(!GetFo3LootPanel().reference, "aim loss closes loot list");
    Check(!activate() && GetFo3LootPanel().rows.empty(),
          "activation without current selection cannot loot stale row");
    fo3loot::Cursor cursor;
    cursor.Target(40, 10);
    cursor.Scroll(-1, 1, 10);
    Check(cursor.selected == 0,
          "entering a target with held stick waits for neutral");
    cursor.Scroll(0, 1, 10);
    cursor.Scroll(-1, 1.1, 10);
    cursor.Scroll(-1, 1.2, 10);
    Check(cursor.selected == 1, "scroll debounces held input");
    cursor.Scroll(-1, 1.46, 10);
    Check(cursor.selected == 2, "held scroll repeats after delay");
    cursor.Target(41, 1);
    Check(cursor.selected == 0 && !cursor.armed,
          "new container resets selection and neutral guard");
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
