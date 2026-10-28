#include "player/fo3-player-state.h"
#include "ui/pipboy/fo3-pipboy-state.h"
#include "npc/fo3-package-schedule.h"
#include "data/fo3-esm-reader.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <unistd.h>
using namespace fo3pipdata;
static fo3player::Catalog Fixture() {
  fo3player::Catalog c;
  c.initial.baseHealth = 100;c.initial.special.fill(1);
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
static void SkillPresentationDecodeTests() {
  assert(Relevant("AVIF"));
  Definitions d;
  auto sub=[](std::vector<uint8_t>& out,const char* type,
              const std::string& text){
    out.insert(out.end(),type,type+4);
    const auto n=uint16_t(text.size()+1);
    out.push_back(uint8_t(n));out.push_back(uint8_t(n>>8));
    out.insert(out.end(),text.begin(),text.end());out.push_back(0);
  };
  std::vector<uint8_t> barter;
  sub(barter,"EDID","AVBarter");
  sub(barter,"FULL","Barter");
  sub(barter,"DESC","Original AVIF skill description");
  sub(barter,"ICON","Interface\\Icons\\Stats\\Skills_Barter.dds");
  Decode(d,"AVIF",100,0,barter,0,0,0);
  assert(d.skills[0].name=="Barter");
  assert(d.skills[0].description=="Original AVIF skill description");
  assert(d.skills[0].icon=="Interface\\Icons\\Stats\\Skills_Barter.dds");
  assert(d.skills[1].icon.empty());
  std::vector<uint8_t> unarmed;
  sub(unarmed,"EDID","AVUnarmed");
  sub(unarmed,"ICON","Interface\\Icons\\Stats\\Skills_Unarmed.dds");
  Decode(d,"AVIF",101,0,unarmed,0,0,0);
  assert(d.skills[13].icon=="Interface\\Icons\\Stats\\Skills_Unarmed.dds");
  assert(d.skills[12].icon.empty()); // unused Throwing actor value
  std::vector<uint8_t> strength;
  sub(strength,"EDID","AVStrength");
  sub(strength,"FULL","Strength");
  sub(strength,"DESC","Source-defined Strength text");
  sub(strength,"ICON","Interface\\Icons\\Stats\\Special_Strength.dds");
  Decode(d,"AVIF",102,0,strength,0,0,0);
  assert(d.special[0].name=="Strength"&&
         d.special[0].description=="Source-defined Strength text"&&
         d.special[0].icon=="Interface\\Icons\\Stats\\Special_Strength.dds");
  std::vector<uint8_t> luck;
  sub(luck,"EDID","AVLuck");
  sub(luck,"ICON","Interface\\Icons\\Stats\\Special_Luck.dds");
  Decode(d,"AVIF",103,0,luck,0,0,0);
  assert(d.special[6].icon=="Interface\\Icons\\Stats\\Special_Luck.dds");
  assert(d.special[5].icon.empty());
}
static void PerkRecordDecodeTests() {
  Definitions data;
  auto sub=[](std::vector<uint8_t>& out,const char* type,
              const std::vector<uint8_t>& bytes){
    out.insert(out.end(),type,type+4);
    out.push_back(uint8_t(bytes.size()));
    out.push_back(uint8_t(bytes.size()>>8));
    out.insert(out.end(),bytes.begin(),bytes.end());
  };
  std::vector<uint8_t> ct(28,0);
  ct[0]=0x60; // >=
  // required Intelligence >= 4.0; GetPermanentActorValue 495, AV 9
  ct[6]=0x80;ct[7]=0x40;ct[8]=0xEF;ct[9]=0x01;ct[12]=9;
  std::vector<uint8_t> perk;
  sub(perk,"EDID",{'T','e','s','t','P','e','r','k',0});
  sub(perk,"FULL",{'T','e','s','t',0});
  sub(perk,"CTDA",ct); // prerequisite, outside PRKE
  sub(perk,"DATA",{0,4,1,1,0}); // playable, not hidden
  sub(perk,"PRKE",{2,0,0});
  sub(perk,"DATA",{9,3,1}); // XP entry point, multiply
  sub(perk,"PRKC",{0});
  sub(perk,"CTDA",ct); // effect condition, not prerequisite
  sub(perk,"EPFT",{1});
  sub(perk,"EPFD",{0xCD,0xCC,0x8C,0x3F});
  sub(perk,"PRKF",{});
  Decode(data,"PERK",201,0,perk,0,0,0);
  const auto& p=data.perks.at(201);
  assert(p.playable&&!p.hidden&&p.minLevel==4&&p.ranks==1);
  assert(p.prerequisites.size()==1&&p.prerequisites[0].function==495&&
         p.prerequisites[0].a==9);
  assert(p.effects.size()==1&&p.effects[0].entryPoint==9&&
         p.effects[0].conditions.at(0).size()==1);
  auto unavailable=perk;
  // Reconstruct the same record with its authored playable flag off.
  unavailable.clear();
  sub(unavailable,"EDID",{'Q','u','e','s','t','P','e','r','k',0});
  sub(unavailable,"DATA",{0,0,1,0,1});
  Decode(data,"PERK",202,0,unavailable,0,0,0);
  assert(!data.perks.at(202).playable&&data.perks.at(202).hidden);
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
  std::vector<uint8_t> actions=payload;std::vector<uint8_t> actionId;put32(actionId,123);sub(actions,"INAM",actionId);
  Decode(d,"PACK",102,0,actions,0,0,0);assert(d.packages.at(102).procedureActions&&!d.packages.at(102).scripted);
  actions=payload;sub(actions,"TNAM",actionId);Decode(d,"PACK",103,0,actions,0,0,0);assert(d.packages.at(103).procedureActions);
  actions=payload;sub(actions,"INAM",std::vector<uint8_t>(4,0));sub(actions,"TNAM",std::vector<uint8_t>(4,0));Decode(d,"PACK",104,0,actions,0,0,0);assert(!d.packages.at(104).procedureActions);
  std::vector<uint8_t> styled=payload;std::vector<uint8_t> styleId;put32(styleId,1234);sub(styled,"CNAM",styleId);Decode(d,"PACK",105,0,styled,0,0,0);assert(d.packages.at(105).combatStyleValid&&d.packages.at(105).combatStyle==1234);
  styled=payload;sub(styled,"CNAM",std::vector<uint8_t>(3,0));Decode(d,"PACK",106,0,styled,0,0,0);assert(!d.packages.at(106).combatStyleValid);
  styled=payload;sub(styled,"CNAM",std::vector<uint8_t>(4,0));Decode(d,"PACK",107,0,styled,0,0,0);assert(d.packages.at(107).combatStyleValid&&!d.packages.at(107).combatStyle);
  // Original Nathan PACK 00019542 uses the legacy 20-byte CTDA layout.
  std::vector<uint8_t> legacy{0xa0,0,0,0,0,0,4,0x42,77,0,0,0,0,0,0,0,0,0,0,0};
  auto conditioned=payload;sub(conditioned,"CTDA",legacy);Decode(d,"PACK",108,0,conditioned,0,0,0);
  const auto& condition=d.packages.at(108).conditions.at(0);
  assert(condition.flags==0xa0&&condition.value==33&&condition.function==77&&condition.run==0&&condition.reference==0);
  legacy.resize(19);conditioned=payload;sub(conditioned,"CTDA",legacy);Decode(d,"PACK",109,0,conditioned,0,0,0);
  assert(d.packages.at(109).conditions.at(0).function==0);
  d.targets[200].base=1;
  d.packages[100].location.type=0;d.packages[100].location.value=200;
  d.doorBases.insert(0xF00);
  d.targets[201].base=0xF00;d.targets[201].cell=0x9;
  d.targets[202].base=0xF01;
  Finalize(d);
  assert(d.targets.count(200));
  assert(d.targets.count(201)&&d.doors.count(0x9));
  assert(!d.targets.count(202));
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
  // Both reciprocal Common House doors must survive target finalization.
  assert(d.targets.count(0x435C)&&d.targets.count(0x43AD));
  assert(d.doorBases.count(d.targets.at(0x435C).base));
  assert(d.doorBases.count(d.targets.at(0x43AD).base));
  assert(c.references.count(0x435C)&&c.references.count(0x43AD));
  // Real original-data package and XTEL anchors at midday.
  assert(d.targets.at(0x43AD).cell==0x4357);
  assert(d.targets.at(0x435C).world==0xA74);
  assert(!c.references.at(0x43AD).locked&&!c.references.at(0x435C).locked);
  assert(d.targets.count(0x1FB87)&&d.targets.at(0x1FB87).world==0xA74);
  assert(d.targets.count(0x66F1E)&&d.targets.at(0x66F1E).world==0xA74);
  assert(d.packages.count(0x1944C)&&d.packages.count(0x2594A));
  const auto& day=d.packages.at(0x1944C);
  const auto& brahmin=d.packages.at(0x2594A);
  assert(day.type==12&&day.location.valid&&day.location.type==0&&
         day.location.value==0x1FB87&&day.schedule.hour==10&&day.schedule.duration==4);
  assert(brahmin.type==12&&brahmin.location.valid&&brahmin.location.type==0&&
         brahmin.location.value==0x66F1E&&brahmin.schedule.hour==8&&brahmin.schedule.duration==12);
  assert(fo3schedule::Active(day.schedule,12.f,{2277,7,17}));
  assert(fo3schedule::Active(brahmin.schedule,12.f,{2277,7,17}));
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
  assert(c.pipboy.discoveryXP==10&&c.pipboy.discoveryRadius==1000);
  const auto caveExit=ExteriorEntrance(c.pipboy,0x24512);assert(caveExit.world==0x3c);
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
static fo3player::Catalog LocationFixture(){
  auto c=Fixture();c.rules.carryBase=100;c.pipboy.markers.clear();
  auto& world=c.pipboy.worlds[4];world.nwX=-10;world.nwY=10;world.seX=10;world.seY=-10;world.width=world.height=2048;
  c.cellOwnership[99].flags=0;c.cellOwnership[98].flags=1;c.cellOwnership[97].flags=5;c.cellOwnership[96].flags=4;
  auto marker=[&](uint32_t id,float x,uint32_t world){auto& m=c.pipboy.markers[id];m.name="Source marker "+std::to_string(id);m.world=world;m.cell=99;m.x=x;m.type=10;};
  marker(3,0,4);marker(5,10000,4);marker(6,0,7);marker(8,15000,4);marker(9,20000,4);marker(10,25000,4);
  c.pipboy.markers[5].radius=50;c.pipboy.markers[8].mapFlags=3;c.pipboy.markers[9].mapFlags=4;c.pipboy.markers[10].flags=0x800;
  c.pipboy.worlds[7]=world;c.pipboy.worlds[7].parent=4;c.pipboy.worlds[7].parentFlags=4;
  c.pipboy.formNames["marker5"]=5;return c;
}
static void LocationDefinitionTests(){
  // The catalog always sends GMST to Decode, independently of Relevant().
  // GMST must remain in its original fingerprint/stat-rule classification.
  assert(!Relevant("GMST"));Definitions d;
  auto setting=[&](const std::string& name,uint32_t bits){
    std::vector<uint8_t> bytes;
    auto sub=[&](const char* tag,const std::vector<uint8_t>& data){bytes.insert(bytes.end(),tag,tag+4);bytes.push_back(uint8_t(data.size()));bytes.push_back(uint8_t(data.size()>>8));bytes.insert(bytes.end(),data.begin(),data.end());};
    std::vector<uint8_t> text(name.begin(),name.end());text.push_back(0);sub("EDID",text);
    sub("DATA",{uint8_t(bits),uint8_t(bits>>8),uint8_t(bits>>16),uint8_t(bits>>24)});
    Decode(d,"GMST",123,0,bytes,0,0,0);
  };
  setting("iXPRewardDiscoverMapMarker",25);assert(d.discoveryXP==25);
  setting("iMapMarkerRevealDistance",500);assert(d.discoveryRadius==500);
  uint32_t bits;const float zoom=3;std::memcpy(&bits,&zoom,4);setting("fWorldMapMaxZoom",bits);assert(d.worldZoomMax==3);
}
static void LocationTests(){
  auto c=LocationFixture();fo3player::Player p(c);auto& d=p.Definitions().pipboy;
  assert(!p.TrackLocations(0,0,0,0)&&!p.TrackLocations(4,NAN,0,0));
  assert(!p.TrackLocations(4,1001,0,0)); // Not an occupied-cell rectangle.
  assert(!p.TrackLocations(4,0,0,1001)); // Original engine uses 3D distance.
  assert(p.RevealMapMarker(3,false)&&!MarkerKnown(d.markers.at(3),3,p.Snapshot().pipboy));
  assert(MarkerVisible(d.markers.at(3),3,p.Snapshot().pipboy));
  assert(p.TrackLocations(4,999,0,0)==1&&p.Snapshot().experience==10);
  assert(!p.Snapshot().pipboy.discovered.count(6)); // Shared parent map is not the physical world.
  assert(!p.TrackLocations(4,0,0,0)&&p.Snapshot().experience==10);
  uint32_t notice=0;fo3player::ExperienceNotice xp;
  assert(p.PollLocationNotice(notice)&&notice==3&&!p.PollLocationNotice(notice));
  assert(p.PollExperienceNotice(xp)&&xp.amount==10&&!p.PollExperienceNotice(xp));
  assert(!p.TrackLocations(4,10051,0,0)&&p.TrackLocations(4,10049,0,0)==1);
  assert(!p.TrackLocations(4,15000,0,0)); // Authored initially-travelable marker: no free XP.
  assert(!p.TrackLocations(4,25000,0,0)); // Initially disabled.
  std::string reason;assert(p.CanFastTravel(3,99,4,reason));
  assert(!p.CanFastTravel(9,99,4,reason)&&reason.find("not discovered")!=reason.npos);
  assert(!p.CanFastTravel(3,98,0,reason)&&p.CanFastTravel(3,97,0,reason));
  assert(!p.CanFastTravel(3,96,4,reason)); // Authored inverse exterior restriction.
  auto blockedCatalog=c;blockedCatalog.pipboy.worlds[4].flags=2;fo3player::Player blocked(blockedCatalog);
  assert(blocked.Discover(3)&&!blocked.CanFastTravel(3,99,4,reason));
  p.SetFastTravelEnabled(false);assert(!p.CanFastTravel(3,99,4,reason));
  const float xpBefore=float(p.Snapshot().experience);
  assert(p.TrackExteriorLocation({99,4,120,240,.5f}));
  const auto file="/tmp/fq-location-"+std::to_string(getpid());std::string error;
  assert(p.Save(file,error));fo3player::Player restored(c);assert(restored.Restore(file,error));std::remove(file.c_str());
  assert(!restored.Snapshot().pipboy.fastTravelEnabled&&restored.Snapshot().pipboy.lastExterior.x==120);
  assert(restored.Snapshot().pipboy.revealed.count(3)&&restored.Snapshot().pipboy.discovered.count(3));
  assert(!restored.TrackLocations(4,0,0,0)&&float(restored.Snapshot().experience)==xpBefore);
  assert(!restored.PollLocationNotice(notice)&&!restored.PollExperienceNotice(xp));
  std::vector<uint8_t> saved;EncodeState(p.Snapshot().pipboy,saved);
  size_t extension=saved.size();for(size_t at=0;at+4<=saved.size();++at)if(fo3esm::ReadU32(saved.data()+at)==0x4d415031)extension=at;
  assert(extension<saved.size());SessionState legacy;assert(DecodeState(legacy,c.pipboy,saved.data(),extension,error));
  assert(legacy.discovered.count(3)&&legacy.fastTravelEnabled&&legacy.revealed.empty());
  assert(!DecodeState(legacy,c.pipboy,saved.data(),saved.size()-1,error)&&legacy.discovered.count(3));
  fo3player::Player dev(c);const auto count=dev.DebugUnlockLocations();
  assert(count==3&&dev.Snapshot().experience==0&&dev.Snapshot().pipboy.discovered.empty());
  assert(!MarkerKnown(c.pipboy.markers.at(9),9,dev.Snapshot().pipboy));
  assert(!MarkerKnown(c.pipboy.markers.at(10),10,dev.Snapshot().pipboy));
  assert(!dev.PollLocationNotice(notice)&&!dev.PollExperienceNotice(xp));
  assert(!dev.TrackLocations(4,0,0,0)&&dev.DebugUnlockLocations()==0);
  const auto devRevision=dev.Revision();assert(dev.DebugUnlockLocations()==0&&dev.Revision()==devRevision);
  auto rewardCatalog=c;rewardCatalog.pipboy.discoveryXP=25;fo3player::Player reward(rewardCatalog);
  assert(reward.TrackLocations(4,0,0,0)==1&&reward.Snapshot().experience==25);
  auto disabledCatalog=c;disabledCatalog.references[3].base=0x10;disabledCatalog.references[3].cell=99;
  disabledCatalog.initial.referenceEnabled[3]=false;fo3player::Player disabled(disabledCatalog);
  assert(!disabled.MapMarkerEnabled(3)&&!disabled.TrackLocations(4,0,0,0));
  fo3player::Player script(c);ResultScript body;
  body.source="ShowMap Marker5 0\nEnableFastTravel 0";
  assert(script.ExecuteDialogueResult(body,error));
  assert(script.Snapshot().pipboy.revealed.count(5)&&!MarkerKnown(c.pipboy.markers.at(5),5,script.Snapshot().pipboy));
  assert(!script.Snapshot().pipboy.fastTravelEnabled&&script.Snapshot().experience==0);
  body.source="ShowMap Marker5 1\nEnableFastTravel 1";assert(script.ExecuteDialogueResult(body,error));
  assert(MarkerKnown(c.pipboy.markers.at(5),5,script.Snapshot().pipboy)&&script.Snapshot().pipboy.fastTravelEnabled);
  assert(!script.TrackLocations(4,10000,0,0)&&!script.PollLocationNotice(notice));
  auto candidate=script.PrepareDialogueTransaction(ResultScript{},ResultScript{},error);assert(candidate);
  assert(script.CommitScriptTransaction(*candidate)&&script.RollbackScriptTransaction(*candidate));
  // Game time changes only through an explicit successful travel commit hook.
  const auto days=script.Snapshot().gameDaysPassed;const float hour=script.Snapshot().gameHour;
  assert(!script.AdvanceGameHours(NAN)&&script.Snapshot().gameHour==hour);
  assert(script.AdvanceGameHours(25.5)&&script.Snapshot().gameDaysPassed==days+1&&std::abs(script.Snapshot().gameHour-hour-1.5f)<.001f);
}
static void MapControlTests(){
  auto c=LocationFixture();fo3player::Player p(c);assert(p.ExploreLocation(3));
  fo3pip::Menu menu;menu.tab=fo3pip::Tab::Data;menu.page=1;menu.map.mapWorld=4;menu.map.location={99,4,0,0,0};
  menu.map.centre=MapProject(c.pipboy,4,0,0);menu.inPage=true;menu.FocusMapMarker(p);assert(menu.selectedMarker==3);
  menu.Zoom(1,10,c.pipboy);assert(menu.map.zoom==5);menu.Zoom(-1,10,c.pipboy);assert(menu.map.zoom==.75f);
  menu.FocusMapMarker(p);assert(menu.selectedMarker==3);
  assert(!menu.Invoke(fo3pip::Action::Accept,p)&&menu.travelPrompt&&!menu.travelRequest);
  menu.Invoke(fo3pip::Action::Back,p);assert(!menu.travelPrompt&&menu.inPage);
  menu.Invoke(fo3pip::Action::Accept,p);menu.Invoke(fo3pip::Action::Accept,p);assert(menu.travelRequest==3);
  menu.travelRequest=0;menu.travelPrompt=false;
  menu.Invoke(fo3pip::Action::Accept,p);p.SetFastTravelEnabled(false);menu.Invoke(fo3pip::Action::Accept,p);
  assert(!menu.travelRequest&&!menu.travelPrompt&&!menu.actionError.empty());p.SetFastTravelEnabled(true);
  menu.map.centre={.9f,.9f};menu.FocusMapMarker(p);assert(!menu.selectedMarker);
  assert(menu.Invoke(fo3pip::Action::Accept,p)&&p.Snapshot().pipboy.waypoint.world==4);
  // Original XTEL graph anchors an interior on the exterior world map.
  auto defs=c.pipboy;Placement door;door.cell=98;door.teleportRef=77;defs.doors[98].push_back(door);
  defs.targets[77].cell=99;defs.targets[77].world=4;defs.targets[77].x=123;
  assert(ExteriorEntrance(defs,98).x==123&&ExteriorEntrance(defs,98).world==4);
}
int main(int argc, char **argv) {
  StateTests();
  LocationDefinitionTests();
  LocationTests();
  MapControlTests();
  MapTests();
  AidTests();
  PackageDecodeTests();
  PerkRecordDecodeTests();
  SkillPresentationDecodeTests();
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
