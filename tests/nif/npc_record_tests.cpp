#include "fo3-npc.h"
#include "fo3-texture-bsa.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
static std::vector<uint8_t> testMesh;
static Fo3RgbaTexture testTexture;
bool LoadFalloutMeshFile(const std::string &, std::vector<uint8_t> &bytes,
                         std::string *) {
  bytes = testMesh;
  return !bytes.empty();
}
bool LoadFalloutTextureRgba(const std::string &, Fo3RgbaTexture &texture) {
  texture = testTexture;
  return texture.width > 0;
}
using Bytes = std::vector<uint8_t>;
static void U32(Bytes &b, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b.push_back(v >> (i * 8));
}
static void U16(Bytes &b, uint16_t v) {
  b.push_back(v & 255); b.push_back(v >> 8);
}
static void F32(Bytes &b, float v) {
  uint32_t bits=0;std::memcpy(&bits,&v,4);U32(b,bits);
}
static Bytes Word(uint32_t v) {
  Bytes b;
  U32(b, v);
  return b;
}
static Bytes Text(const char *s) { return Bytes(s, s + std::strlen(s) + 1); }
static void Sub(Bytes &b, const char *t, const Bytes &v) {
  b.insert(b.end(), t, t + 4);
  b.push_back(v.size() & 255);
  b.push_back((v.size() >> 8) & 255);
  b.insert(b.end(), v.begin(), v.end());
}
static Bytes Record(const char *t, uint32_t id, const Bytes &data,
                    uint32_t flags = 0) {
  Bytes b;
  b.insert(b.end(), t, t + 4);
  U32(b, data.size());
  U32(b, flags);
  U32(b, id);
  U32(b, 0);
  U32(b, 0);
  b.insert(b.end(), data.begin(), data.end());
  return b;
}
static void Add(Bytes &b, const Bytes &v) {
  b.insert(b.end(), v.begin(), v.end());
}
int main(int argc, char **argv) {
  // FO3's EGT colour image is vertically reversed relative to its DDS.
  testMesh.assign(64, 0);
  std::memcpy(testMesh.data(), "FREGT003", 8);
  testMesh[8] = 2; testMesh[12] = 2; testMesh[16] = 1;
  U32(testMesh, 0x3f800000); // scale 1
  // Planar R/G/B: top row then bottom row in EGT storage.
  const uint8_t channels[] = {1,2,10,20, 3,4,30,40, 5,6,50,60};
  testMesh.insert(testMesh.end(), channels, channels+12);
  testTexture.width = testTexture.height = 2;
  testTexture.rgba = {100,100,100,7, 100,100,100,8,
                      100,100,100,9, 100,100,100,10};
  Fo3FaceGenTextureQ234 generated;
  assert(LoadFo3FaceGenTextureQ234("head.nif", "base.dds", {1}, generated));
  assert((generated.rgba == std::vector<uint8_t>{110,130,150,7,
      120,140,160,8, 101,103,105,9, 102,104,106,10}));
  testMesh.clear(); testTexture = {};
  Bytes esm, race;
  Sub(race, "EDID", Text("TestRace"));
  Sub(race, "NAM0", {});
  Sub(race, "MNAM", {});
  Sub(race, "INDX", Word(0));
  Sub(race, "MODL", Text("male-head.nif"));
  Sub(race, "FNAM", {});
  Sub(race, "INDX", Word(0));
  Sub(race, "MODL", Text("female-head.nif"));
  Sub(race, "NAM1", {});
  Sub(race, "MNAM", {});
  Sub(race, "INDX", Word(0));
  Sub(race, "MODL", Text("male-body.nif"));
  Sub(race, "FNAM", {});
  Sub(race, "INDX", Word(0));
  Sub(race, "MODL", Text("female-body.nif"));
  Add(esm, Record("RACE", 10, race));
  Bytes armor;
  Sub(armor, "EDID", Text("Armor"));
  Sub(armor, "MODL", Text("male-worn.nif"));
  Sub(armor, "MOD2", Text("male-dropped.nif"));
  Sub(armor, "MOD3", Text("female-worn.nif"));
  Sub(armor, "BMDT", Word(4));
  Add(esm, Record("ARMO", 20, armor));
  auto npc = [&](uint32_t id, bool female, uint16_t templateFlags = 0) {
    Bytes b, acbs(24);
    acbs[0] = female ? 1 : 0;
    acbs[22] = templateFlags & 255;
    acbs[23] = templateFlags >> 8;
    Sub(b, "EDID", Text(female ? "Female" : "Male"));
    Sub(b, "ACBS", acbs);
    Sub(b, "RNAM", Word(10));
    Sub(b, "MODL", Text("skeleton.nif"));
    Bytes inv = Word(20);
    U32(inv, 1);
    Sub(b, "CNTO", inv);
    if (templateFlags)
      Sub(b, "TPLT", Word(30));
    Add(esm, Record("NPC_", id, b));
  };
  npc(30, false);
  npc(31, true);
  npc(32, true, 1 | 64 | 256);
  Bytes refs;
  for (uint32_t i = 0; i < 5; ++i) {
    Bytes r;
    Sub(r, "NAME", Word(30 + (i < 3 ? i : 0)));
    Sub(r, "DATA", Bytes(24));
    Add(refs, Record("ACHR", 100 + i, r, i == 3 ? 0x800 : i == 4 ? 0x20 : 0));
  }
  Bytes group;
  group.insert(group.end(), {'G', 'R', 'U', 'P'});
  U32(group, refs.size() + 24);
  U32(group, 77);
  U32(group, 6);
  U32(group, 0);
  U32(group, 0);
  Add(group, refs);
  Add(esm, group);
  Bytes nav,navData;U32(navData,77);U32(navData,3);U32(navData,1);U32(navData,1);
  while(navData.size()<24)navData.push_back(0);
  Sub(nav,"DATA",navData);
  Bytes vertices;F32(vertices,0);F32(vertices,0);F32(vertices,0);
  F32(vertices,100);F32(vertices,0);F32(vertices,0);
  F32(vertices,0);F32(vertices,100);F32(vertices,0);Sub(nav,"NVVX",vertices);
  Bytes triangle;U16(triangle,0);U16(triangle,1);U16(triangle,2);
  U16(triangle,0);U16(triangle,0xffff);U16(triangle,0xffff);U16(triangle,1);U16(triangle,0xabcd);Sub(nav,"NVTR",triangle);
  Bytes external;U32(external,0);U32(external,201);U16(external,7);Sub(nav,"NVEX",external);
  Bytes navRecord=Record("NAVM",200,nav),worldGroup;
  worldGroup.insert(worldGroup.end(),{'G','R','U','P'});U32(worldGroup,navRecord.size()+24);U32(worldGroup,88);U32(worldGroup,1);U32(worldGroup,0);U32(worldGroup,0);
  Add(worldGroup,navRecord);Add(esm,worldGroup);
  const auto path = std::filesystem::temp_directory_path() /
                    "falloutquest-npc-record-tests.esm";
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(esm.data()), esm.size());
  }
  std::vector<Fo3NpcActorQ230> actors;
  assert(LoadFo3CellActors(77, actors, path.string()));
  assert(actors.size() == 3);
  assert(!actors[0].female && actors[0].raceHeadModels[0] == "male-head.nif");
  assert(actors[1].female && actors[1].raceHeadModels[0] == "female-head.nif");
  assert(actors[1].raceBodyModels[0] == "female-body.nif");
  assert(actors[1].inventory[0].modelPath == "female-worn.nif");
  assert(!actors[2].female && actors[2].raceHeadModels[0] == "male-head.nif");
  assert(!LoadFo3CellActors(78, actors, path.string()) && actors.empty());
  std::vector<Fo3NpcNavMeshQ240> navigation;
  assert(LoadFo3NpcNavigationQ240(77,88,navigation,path.string()));
  assert(navigation.size()==1&&navigation[0].formId==200&&navigation[0].cellFormId==77);
  assert(navigation[0].vertices.size()==3&&navigation[0].triangles.size()==1);
  assert(navigation[0].triangles[0].vertex[2]==2&&navigation[0].triangles[0].neighbor[0]==0);
  assert(navigation[0].triangles[0].flags==1u&&navigation[0].triangles[0].coverFlags==0xabcdu&&navigation[0].external.size()==1);
  assert(navigation[0].external[0].navMeshFormId==201&&navigation[0].external[0].triangle==7);
  std::filesystem::remove(path);
  if (argc > 1) {
    assert(LoadFo3CellActors(0xa96, actors, argv[1]));
    assert(actors.size() == 3);
    bool lucas = false;
    for (const auto &a : actors)
      if (a.editorId == "LucasSimms") {
        lucas = true;
        assert(a.raceEditorId == "AfricanAmerican" &&
               a.raceHeadModels.size() == 8);
        assert(a.inventory[1].modelPath == "Armor\\LucasSimms\\M\\OutfitM.NIF");
      }
    assert(lucas);
    std::cout << "Original ESM: 3 Megaton actors resolved\n";
  }
  std::cout << "NPC record tests passed\n";
}
