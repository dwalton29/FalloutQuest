#include "player/fo3-player-state.h"
#include "ui/pipboy/fo3-pipboy-state.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <unistd.h>
using namespace fo3pipdata;
static fo3player::Catalog Fixture() {
  fo3player::Catalog c;
  c.initial.baseHealth = 100;
  c.rules.apBase = 50;
  c.pipboy.perks[1].ranks = 2;
  c.pipboy.quests[2].name = "Authored fixture";
  c.pipboy.quests[2].stages[10] = {};
  c.pipboy.quests[2].objectives[20].text = "Objective fixture";
  c.pipboy.markers[3] = {};
  c.pipboy.worlds[4].valid = true;
  c.pipboy.transmitters[5] = {};
  return c;
}
static void StateTests() {
  auto c = Fixture();
  fo3player::Player p(c);
  assert(p.Snapshot().pipboy.quests.empty());
  assert(!p.GrantPerk(1, 3));
  assert(p.GrantPerk(1, 2));
  assert(!p.SetQuestStage(2, 10));
  assert(p.StartQuest(2));
  assert(p.SetQuestStage(2, 10));
  assert(p.SetObjective(2, 20, true, Completion::Active));
  assert(p.SelectQuest(2));
  assert(p.Discover(3));
  assert(p.SetWaypoint(4, 50, -30));
  assert(p.TuneRadio(5));
  std::string error;
  auto path = "/tmp/fq-pip-data-" + std::to_string(getpid());
  assert(p.Save(path, error));
  fo3player::Player restored(c);
  assert(restored.Restore(path, error));
  auto &s = restored.Snapshot().pipboy;
  assert(s.quests.at(2).stages.count(10));
  assert(s.quests.at(2).objectives.at(20).displayed);
  assert(s.selectedQuest == 2 && s.perks.at(1) == 2 && s.discovered.count(3) &&
         s.tunedRadio == 5 && s.waypoint.point.x == 50);
  assert(restored.FinishQuest(2, Completion::Failed));
  assert(!restored.Snapshot().pipboy.selectedQuest);
  std::remove(path.c_str());
  std::vector<uint8_t> bytes;
  EncodeState(s, bytes);
  SessionState rejected;
  auto count = bytes.size();
  assert(!DecodeState(rejected, c.pipboy, bytes.data(), count - 1, error));
  assert(rejected.quests.empty());
}
static void MapTests() {
  World w;
  w.nwX = -30;
  w.nwY = 30;
  w.seX = 20;
  w.seY = -20;
  w.width = w.height = 2048;
  w.valid = true;
  auto p = w.Project(-30 * 4096, 30 * 4096);
  assert(p.x == 0 && p.y == 0);
  p = w.Project(20 * 4096, -20 * 4096);
  assert(p.x == 1 && p.y == 1);
  for (auto v : {Point{0, 0}, Point{.5f, .5f}, Point{.2f, .9f}}) {
    auto game = w.Unproject(v);
    auto back = w.Project(game.x, game.y);
    assert(std::fabs(back.x - v.x) < 1e-5 && std::fabs(back.y - v.y) < 1e-5);
  }
  Definitions d;
  d.worlds[1] = w;
  d.worlds[2].parent = 1;
  d.worlds[2].parentFlags = 4;
  assert(MapWorld(d, 2) == 1);
  d.worlds[2].scale = 2;
  d.worlds[2].offsetX = 3;
  d.worlds[2].offsetY = -4;
  auto child = MapProject(d, 2, 4096, 8192), expected = w.Project(5 * 4096, 0);
  assert(std::fabs(child.x - expected.x) < 1e-5 &&
         std::fabs(child.y - expected.y) < 1e-5);
  auto root = WorldPoint(d, 2, 4096, 8192);
  assert(root.x == 5 * 4096 && root.y == 0);
  d.worlds[2].scale = 1;
  d.worlds[2].offsetX = d.worlds[2].offsetY = 0;
  d.worlds[1].parent = 2;
  d.worlds[1].parentFlags = 4;
  assert(!MapWorld(d, 2));
  Transmitter t;
  t.radius = 100;
  t.world = 2;
  t.x = 50;
  Location l;
  l.world = 2;
  l.x = 50;
  assert(InRange(d, t, l));
  l.x = 151;
  assert(!InRange(d, t, l));
  t.flags = 0x800;
  assert(!InRange(d, t, l));
}
static void AidTests() {
  auto c = Fixture();
  fo3player::Item item;
  item.formId = 11;
  item.kind = fo3player::ItemKind::Aid;
  c.items[11] = item;
  c.pipboy.magic[12] = {0, 0, 16, 0};
  c.pipboy.aid[11].effects.push_back({12, 20, 0, 0, 0, 16, false});
  fo3player::Player p(c);
  p.Add(11, 2);
  p.DamageHealth(30);
  auto id = p.Snapshot().inventory.front().id;
  assert(p.Use(id));
  assert(p.Health() == 90 && p.Snapshot().inventory.front().count == 1);
  c.pipboy.aid[11].effects.push_back({12, 5, 0, 10, 0, 16, false});
  fo3player::Player blocked(c);
  blocked.Add(11, 2);
  auto rev = blocked.Revision();
  assert(!blocked.Use(blocked.Snapshot().inventory.front().id));
  assert(blocked.Revision() == rev &&
         blocked.Snapshot().inventory.front().count == 2);
}
static void PackageDecodeTests() {
  Definitions d;
  auto put32=[](std::vector<uint8_t>& b,uint32_t v){for(int i=0;i<4;++i)b.push_back(uint8_t(v>>(i*8)));};
  auto sub=[&](std::vector<uint8_t>& b,const char* name,const std::vector<uint8_t>& value){
    b.insert(b.end(),name,name+4);b.push_back(uint8_t(value.size()));b.push_back(uint8_t(value.size()>>8));
    b.insert(b.end(),value.begin(),value.end());
  };
  std::vector<uint8_t> payload;
  sub(payload,"EDID",std::vector<uint8_t>{'A','u','t','h','o','r','e','d',0});
  std::vector<uint8_t> pkdt;put32(pkdt,0x600);pkdt.push_back(12);pkdt.push_back(0);pkdt.push_back(0xcd);pkdt.push_back(0);
  put32(pkdt,0);sub(payload,"PKDT",pkdt);
  std::vector<uint8_t> pldt;put32(pldt,3);put32(pldt,0);put32(pldt,4000);sub(payload,"PLDT",pldt);
  std::vector<uint8_t> psdt{0xff,0xff,0,0xff};put32(psdt,0);sub(payload,"PSDT",psdt);
  sub(payload,"SCHR",std::vector<uint8_t>(20,0));
  Decode(d,"PACK",100,0,payload,0,0,0);
  const auto& p=d.packages.at(100);
  assert(p.editor=="Authored"&&p.type==12&&p.location.valid&&p.location.type==3&&
         p.location.radius==4000&&p.schedule.valid&&p.schedule.hour==-1&&!p.scripted);
  std::vector<uint8_t> scripted=payload;sub(scripted,"SCTX",std::vector<uint8_t>{'s','e','t',' ', 'x',0});
  Decode(d,"PACK",101,0,scripted,0,0,0);
  assert(d.packages.at(101).scripted);
  d.targets[200].base=1;
  d.packages[100].location.type=0;d.packages[100].location.value=200;
  Finalize(d);
  assert(d.targets.count(200));
}
static void RadioTests() {
  Definitions d;
  d.stations[30].name = "Fixture station";
  d.transmitters[5].base = 30;
  d.radioHello = 9;
  d.quests[2].editor = "Q";
  d.quests[2].script = 40;
  Condition c;
  c.function = 72;
  c.a = 30;
  c.value = 1;
  d.quests[2].conditions.push_back(c);
  d.scripts[40].variables[1] = "Last";
  d.sounds[50] = "sound/fixture.wav";
  Info i;
  i.id = 8;
  i.topic = 9;
  i.quest = 2;
  i.type = 7;
  i.responses.push_back({50, 1, "Fixture"});
  i.scripts.push_back("set Q.Last to ( Q.Last + 1 )");
  c.function = 79;
  c.a = 2;
  c.b = 1;
  c.value = 2;
  c.flags = 128;
  i.conditions.push_back(c);
  d.topics[9].push_back(i);
  Broadcast b;
  SessionState state;
  std::string error;
  assert(b.Tune(d, 5));
  assert(b.Advance(d, state, error).front() == "sound/fixture.wav");
  assert(b.Advance(d, state, error).size() == 1);
  assert(b.Advance(d, state, error).empty() && !error.empty());
  assert(b.Tune(d, 5));
  assert(!b.Advance(d, state, error).empty());
  Condition unsupported;
  unsupported.function = 600;
  assert(!b.Conditions(d, state, {unsupported}));
  assert(!b.Assign(d, {"SetStage Q 10"}, true));
  assert(b.Tune(d, 0));
  assert(b.Advance(d, state, error).empty());
}
static void MenuDataTests() {
  auto c = Fixture();
  fo3player::Item item;
  item.formId = 100;
  item.kind = fo3player::ItemKind::Note;
  item.playable = true;
  c.items[100] = item;
  c.pipboy.notes[100].name = "Note fixture";
  c.pipboy.notes[100].type = 1;
  c.pipboy.notes[100].text = "Full text";
  fo3player::Player p(c);
  fo3pip::Menu menu;
  menu.tab = fo3pip::Tab::Data;
  menu.page = 3;
  menu.Refresh(p);
  assert(menu.rows.empty());
  p.Add(100, 1);
  menu.Refresh(p);
  assert(menu.rows.size() == 1 && menu.rows[0] == 100);
  menu.Invoke(fo3pip::Action::Accept, p);
  assert(menu.inPage);
  menu.Invoke(fo3pip::Action::Down, p);
  assert(menu.textScroll == 1);
  menu.Invoke(fo3pip::Action::Back, p);
  assert(!menu.inPage);
  menu.tab = fo3pip::Tab::Items;
  menu.page = 3;
  menu.RebuildRows(p);
  assert(menu.rows.empty());
  c.pipboy.quests[2].objectives[20].targets = {77};
  c.pipboy.targets[77].world = 4;
  fo3player::Player quest(c);
  quest.StartQuest(2);
  quest.SetObjective(2, 20, true, Completion::Active);
  quest.SelectQuest(2);
  assert(QuestTargets(c.pipboy, quest.Snapshot().pipboy) ==
         std::vector<uint32_t>{77});
  quest.SetObjective(2, 20, true, Completion::Complete);
  assert(QuestTargets(c.pipboy, quest.Snapshot().pipboy).empty());
  quest.FinishQuest(2, Completion::Complete);
  assert(quest.Snapshot().pipboy.quests.at(2).status == Completion::Complete);
}
static void Original(const char *path) {
  fo3player::Catalog c;
  std::string error;
  if (!fo3player::LoadCatalog(path, c, error)) {
    std::cerr << error << '\n';
    std::abort();
  }
  auto &d = c.pipboy;
  assert(d.notes.size() == 840 && d.perks.size() == 87 &&
         d.quests.size() == 192);
  assert(d.radiationStages.size() == 5);
  assert(d.radiationStages.at(0x335a2).threshold == 200 &&
         d.radiationStages.at(0x335a2).spell == 0x32dee);
  auto &w = d.worlds.at(0x3c);
  const auto &megaton = d.markers.at(0x62743);
  assert(megaton.name == "Megaton" && megaton.type == 1 &&
         megaton.mapFlags == 0 && megaton.world == 0xa74 &&
         MapWorld(d, megaton.world) == 0x3c && megaton.base == 0x10);
  assert(w.image == "Interface\\Worldmap\\Wasteland_1024_no_map.dds" &&
         w.nwX == -30 && w.nwY == 30 && w.seX == 20 && w.seY == -20 &&
         w.width == 2048);
  assert(d.stations.count(0x22487) && d.stations.count(0x18774) &&
         !d.stations.count(0x335a2));
  assert(d.notes.at(0x7e3bf).text.find("Capital Post") != std::string::npos);
  auto &note = d.notes.at(0x52394);
  assert(note.type == 3 && note.npc == 0x824ab && note.topic == 0x52390);
  auto audio = NoteAudio(d, 0x52394);
  assert(!audio.empty() && audio.front().rfind("@voice:", 0) == 0);
  fo3player::Player p(c);
  assert(p.Snapshot().pipboy.quests.empty() &&
         p.Snapshot().pipboy.perks.empty());
  p.Add(0x151a3, 2);
  uint64_t water = 0;
  for (auto &s : p.Snapshot().inventory)
    if (s.formId == 0x151a3)
      water = s.id;
  p.DamageHealth(30);
  assert(p.Use(water));
  assert(p.Health() == p.MaxHealth() - 10);
  Broadcast radio;
  radio.random.seed(4);
  assert(radio.Tune(d, 0x22488));
  size_t segments = 0;
  for (int i = 0; i < 12; ++i) {
    auto paths = radio.Advance(d, p.Snapshot().pipboy, error);
    if (paths.empty())
      break;
    for (auto &path : paths)
      assert(path.rfind("sound/", 0) == 0 || path.rfind("@voice:", 0) == 0);
    ++segments;
  }
  assert(segments > 0);
  std::cout << "Original ESM maps=" << d.worlds.size()
            << " markers=" << d.markers.size() << " quests=" << d.quests.size()
            << " notes=" << d.notes.size() << " stations=" << d.stations.size()
            << " broadcast segments=" << segments << " stop=" << error << '\n';
}
int main(int argc, char **argv) {
  StateTests();
  MapTests();
  AidTests();
  PackageDecodeTests();
  RadioTests();
  MenuDataTests();
  if (argc > 1)
    Original(argv[1]);
  if (argc > 2) {
    fo3player::Catalog c;
    std::string error;
    assert(fo3player::LoadCatalog(argv[1], c, error));
    fo3player::Player p(c);
    assert(p.Restore(argv[2], error));
    std::cout << "Legacy fingerprints=" << c.fingerprint << ","
              << c.worldFingerprint << "," << c.lootFingerprint
              << " HP=" << p.Health() << " AP=" << p.ActionPoints()
              << " inventory=" << p.Snapshot().inventory.size()
              << " collected=" << p.Snapshot().collected.size()
              << " containers=" << p.Snapshot().containers.size()
              << " weight=" << p.InventoryWeight() << '\n';
  }

  std::cout << "Pip-Boy data/state/map/Aid/broadcast tests passed\n";
}
