#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include "data/fo3-bsa-archive.h"
#include "ui/interaction/fo3-font-layout.h"
#include "ui/pipboy/fo3-pipboy-mesh.h"
#include "ui/pipboy/fo3-pipboy-assets.h"
#include <cassert>
#include <fstream>
#include <iostream>
bool LoadFalloutMeshFile(const std::string &p, std::vector<uint8_t> &b,
                         std::string *resolved) {
  std::ifstream f(p, std::ios::binary);
  b.assign(std::istreambuf_iterator<char>(f), {});
  if (resolved)
    *resolved = p;
  return !b.empty();
}
int main(int argc, char **argv) {
  Fo3StaticNifMesh screen;
  screen.shapeName = "pipboyscreen:0";
  screen.diffuseTexturePath = "textures\\pipboy3000\\Screen.dds";
  screen.noLighting = true;
  screen.positions = {0, 0, 0, 2, 0, 0, 0, 1, 0};
  screen.normals = {0, 0, 1, 0, 0, 1, 0, 0, 1};
  screen.texcoords = {.01f, .02f, .75f, .02f, .01f, .76f};
  auto surface = fo3pip::Inspect(screen);
  assert(surface.valid && surface.normal.z > .99f);
  auto positions = screen.positions;
  fo3pip::ScreenUvs(screen, surface);
  assert(positions == screen.positions);
  assert(screen.texcoords[0] == 0 && screen.texcoords[1] == 1 &&
         screen.texcoords[2] == 1 && screen.texcoords[5] == 0);
  screen.shapeName = "ScreenLit:8";
  assert(!fo3pip::Screen(screen));
  screen.shapeName = "glare:0";
  assert(!fo3pip::Screen(screen));
  auto bind = fo3anim::Identity();
  bind[12] = 12;
  bind[13] = 3;
  bind[14] = 94;
  auto transformed = fo3pip::BindInRenderCoordinates(bind, 70, -1.55f, 0);
  auto vertex =
      fo3anim::Point(transformed, {2.f / 70, -1.55f + 1.f / 70, -3.f / 70});
  assert(std::fabs(vertex[0] - 14.f / 70) < 1e-5f &&
         std::fabs(vertex[1] - (-1.55f + 95.f / 70)) < 1e-5f &&
         std::fabs(vertex[2] - (-6.f / 70)) < 1e-5f);
  if (argc >= 4) {
    std::vector<Fo3StaticNifMesh> meshes;
    assert(LoadFo3StaticNifMeshes(argv[1], meshes));
    unsigned found = 0, glass = 0, controls = 0;
    for (auto &mesh : meshes) {
      if (fo3pip::Screen(mesh)) {
        ++found;
        auto s = fo3pip::Inspect(mesh);
        assert(s.valid && mesh.positions.size() == 56 * 3);
        assert(std::fabs(s.lo[0] + .0104533f) < 1e-5f &&
               std::fabs(s.hi[0] - .752926f) < 1e-5f);
        assert(mesh.shapeBlock == 76);
        std::cout << "screen block=" << mesh.shapeBlock
                  << " normal=" << s.normal.x << "," << s.normal.y << ","
                  << s.normal.z << "\n";
      }
      if (mesh.shapeName == "glare:0")
        ++glass;
      if (mesh.shapeName.find("PipBoyButton") != std::string::npos ||
          mesh.shapeName.find("Knob") != std::string::npos)
        ++controls;
    }
    assert(found == 1 && glass == 1 && controls >= 5);
    std::vector<uint8_t> bytes;
    LoadFalloutMeshFile(argv[1], bytes, nullptr);
    NifHeader h;
    assert(ParseHeader(bytes, h));
    for (uint32_t block = 0; block < h.numBlocks; ++block)
      assert(BlockType(h, block).find("Controller") == std::string::npos);
    LoadFalloutMeshFile(argv[2], bytes, nullptr);
    fo3anim::Skeleton skeleton;
    assert(fo3anim::DecodeSkeleton(bytes, skeleton));
    assert(fo3anim::FindBone(skeleton, "Bip01 L ForeTwist") >= 0);
    fo3assets::BsaArchive misc(argv[3]);
    for (const auto *path :
         {"menus\\globals.xml", "menus\\main\\stats_menu.xml",
          "menus\\main\\inventory_menu.xml", "menus\\main\\map_menu.xml",
          "menus\\prefabs\\card_info.xml",
          "menus\\prefabs\\list_box_template.xml"})
      {assert(misc.Read(path, bytes));
       if(std::string(path).find("prefabs")==std::string::npos)
        assert(fo3pip::MenuAssetValid(path,std::string(bytes.begin(),bytes.end())));
      }
    for (int i = 4; i < argc; ++i) {
      LoadFalloutMeshFile(argv[i], bytes, nullptr);
      fo3font::Metrics font;
      assert(fo3font::ParseFalloutFont(bytes, font));
      assert(font.baseLine == 31 || font.baseLine == 36);
    }
    std::cout << "Original screen/glass/control/forearm/XML/font verification "
                 "passed\n";
  }
  std::cout << "Pip-Boy semantic screen, geometry preservation, UV and "
               "floor-frame tests passed\n";
}
