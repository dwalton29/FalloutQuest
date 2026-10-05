#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include "weapons/fo3-weapon-assets.h"
#include "weapons/fo3-weapon-asset.h"
#include <cassert>
#include <fstream>
#include <iostream>
bool LoadFalloutMeshFile(const std::string &path, std::vector<uint8_t> &bytes,
                         std::string *resolved) {
  std::ifstream f(path, std::ios::binary);
  bytes.assign(std::istreambuf_iterator<char>(f), {});
  if (resolved) *resolved = path;
  return !bytes.empty();
}
std::vector<uint8_t> Read(const std::string &path) {
  std::vector<uint8_t> bytes;
  assert(LoadFalloutMeshFile(path, bytes, nullptr));
  return bytes;
}
void Synthetic() {
  using namespace fo3anim;
  Skeleton s;
  for (const char *name : {"root", "Bip01 R Hand", "Bip01 L Hand", "Weapon"}) {
    Bone b;b.name = name;b.parent = s.bones.empty() ? -1 : 0;s.bones.push_back(b);
  }
  assert(FinalizeSkeleton(s));
  Clip clip;clip.stop = 1;
  for (int i = 1; i < 4; ++i) {
    Track t;t.bone = s.bones[i].name;t.hasTranslation = true;
    t.base.translation = {float(i), 0, 0};clip.tracks.push_back(t);
  }
  fo3weapon::Attachment a;
  assert(fo3weapon::BuildAttachment(s, clip, true, a)&&a.support);
  assert(std::fabs(a.weaponInRight[12]-2)<1e-5f);
  assert(std::fabs(a.leftInWeapon[12]+1)<1e-5f);
  clip.tracks.pop_back();assert(!fo3weapon::BuildAttachment(s, clip, true, a));
  fo3weapon::ModelNodes m;assert(!fo3weapon::DecodeModelNodes({},m));
  assert(m.MeshPart(0)==fo3weapon::Part::Body);
  fo3weapon::Definition d;d.animation=6;
  assert(fo3weapon::AimPath(d)=="meshes/characters/_1stperson/2haaim.kf");
  d.animation=9;assert(fo3weapon::AimPath(d).empty());
}
int main(int argc, char **argv) {
  Synthetic();
  if (argc == 2) {
    const std::string root = argv[1];
    fo3anim::Skeleton skeleton;
    assert(fo3anim::DecodeSkeleton(Read(root+"/skeleton.nif"),skeleton));
    for (int sample : {0,1,2}) {
      const bool rifle=sample!=0,assault=sample==2;
      const std::string model = root+(assault?"/assaultrifle.nif":rifle?"/huntingrifle.nif":"/10mmpistol.nif");
      const std::string aim = root+(assault?"/2haaim.kf":rifle?"/2hraim.kf":"/1hpaim.kf");
      const std::string reload = root+(assault?"/2hareloada.kf":rifle?"/2hrreloada.kf":"/1hpreloada.kf");
      fo3anim::Clip aimClip,reloadClip;
      assert(fo3anim::DecodeClip(Read(aim),aimClip));
      assert(fo3anim::DecodeClip(Read(reload),reloadClip));
      fo3weapon::Asset runtime;assert(runtime.Decode(Read(model),Read(root+"/skeleton.nif"),Read(aim)));
      if(assault){assert(runtime.Reload(Read(reload)));fo3weapon::Definition definition;definition.animation=6;definition.reload=0;assert(runtime.ReloadInteraction(definition)==fo3weapon::ReloadFamily::DetachableRifle);}
      if(!rifle){assert(runtime.Reload(Read(reload)));assert(runtime.reloadSounds[0]=="WPNPistol10mmReloadOut");assert(runtime.reloadSounds[1]=="WPNPistol10mmReloadIn");assert(runtime.reloadSounds[2]=="WPNPistol10mmReloadChamber");}
      const auto hip=fo3weapon::Hip({1,2,3},fo3vr::Identity());assert(hip.Contains(hip.center));
      fo3weapon::Attachment attachment;
      assert(fo3weapon::BuildAttachment(skeleton,aimClip,rifle,attachment));
      assert(attachment.support==rifle);
      const auto identity=fo3anim::Multiply(attachment.weaponInRight,attachment.rightInWeapon);
      for(size_t i=0;i<identity.size();++i)
        assert(std::fabs(identity[i]-fo3anim::Identity()[i])<1e-4f);
      fo3weapon::ModelNodes nodes;
      assert(fo3weapon::DecodeModelNodes(Read(model),nodes));
      // Static weapons have no embedded UI sequence. Preserve the UI decoder's
      // strict requirements instead of weakening that existing path.
      fo3anim::Skeleton ui;std::vector<int> uiBlocks;std::vector<fo3anim::Clip> uiClips;
      assert(!fo3anim::DecodeUiAnimation(Read(model),ui,uiBlocks,uiClips));
      assert(nodes.magazine>=0&&(rifle?nodes.bolt>=0:nodes.slide>=0));
      const auto forward=nodes.MuzzleForward();
      assert(forward[0]>.999f&&std::fabs(forward[1])<1e-4f&&std::fabs(forward[2])<1e-4f);
      const auto &muzzle=nodes.hierarchy.bindGlobal.at(nodes.muzzle);
      assert(std::fabs(muzzle[12]-(assault?45.7826f:rifle?61.306103f:17.79352f))<.001f);
      std::vector<Fo3StaticNifMesh> meshes;
      assert(LoadFo3StaticNifMeshes(model,meshes));
      size_t magazines=0,actions=0;
      for(const auto&m:meshes) {
        const auto part=nodes.MeshPart(m.shapeBlock);
        magazines+=part==fo3weapon::Part::Magazine;
        actions+=part==fo3weapon::Part::Slide||part==fo3weapon::Part::Bolt;
      }
      assert(magazines&&actions);
      if (!rifle) {
        assert(std::fabs(reloadClip.stop-reloadClip.start-1.3f)<1e-4f);
        bool out=false,in=false,chamber=false;
        for(const auto&key:reloadClip.textKeys) {
          if(key.text.find("WPNPistol10mmReloadOut")!=std::string::npos)
            out=std::fabs(key.time-1.f/15)<1e-5f;
          if(key.text.find("WPNPistol10mmReloadIn")!=std::string::npos)
            in=std::fabs(key.time-13.f/30)<1e-5f;
          if(key.text.find("WPNPistol10mmReloadChamber")!=std::string::npos)
            chamber=std::fabs(key.time-.8f)<1e-5f;
          std::cout<<key.time<<" "<<key.text<<"\n";
        }
        assert(out&&in&&chamber);
      }
      std::cout<<model<<": original muzzle, attachment, magazine/action meshes verified\n";
    }
  }
  std::cout<<"Weapon asset tests passed\n";
}
