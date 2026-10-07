#include <cassert>
#include <fstream>
#include <iostream>
#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include "../../app/src/main/cpp/npc/fo3-facial-data.h"
#include "../../app/src/main/cpp/npc/fo3-npc-state.h"
bool LoadFalloutMeshFile(const std::string& path,std::vector<uint8_t>& bytes,std::string* resolved) {
  std::ifstream f(path,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(f),{});if(resolved)*resolved=path;return !bytes.empty();
}
int main(int argc,char** argv) {
  if(argc<4)return 0; // Proprietary originals remain external.
  std::vector<Fo3StaticNifMesh> meshes;assert(LoadFo3StaticNifMeshes(argv[1],meshes));
  std::vector<uint8_t> bytes;assert(LoadFalloutMeshFile(argv[2],bytes,nullptr));fo3face::Tri tri;std::string error;assert(fo3face::DecodeTri(bytes,tri,error));
  assert(LoadFalloutMeshFile(argv[3],bytes,nullptr));fo3face::Lip lip;assert(fo3face::DecodeLip(bytes,lip,error));
  size_t matched=0;
  fo3npc::RuntimeState actor;actor.position={123,45,67};actor.yaw=.7f;const auto root=actor.position;
  for(const auto& mesh:meshes) {
    if(mesh.positions.size()/3!=tri.vertices)continue;
    ++matched;
    std::vector<float> bind(mesh.indices.size()*18,0),output;fo3face::ExpandedDeltas deltas;
    for(size_t v=0;v<mesh.indices.size();++v)for(size_t axis=0;axis<3;++axis){
      bind[v*18+axis]=mesh.positions[mesh.indices[v]*3+axis];
      if(mesh.normals.size()==tri.vertices*3)bind[v*18+3+axis]=mesh.normals[mesh.indices[v]*3+axis];
      bind[v*18+6+axis]=axis==0;bind[v*18+9+axis]=axis==1;
    }
    for(size_t c=0;c<fo3face::MorphNames.size();++c){auto m=tri.morphs.find(fo3face::MorphNames[c]);if(m==tri.morphs.end())continue;
      for(auto vertex:mesh.indices){const auto& d=m->second[vertex];const auto* t=mesh.geometryDeltaToModel;
        deltas[c].push_back({t[0]*d[0]+t[1]*d[1]+t[2]*d[2],t[3]*d[0]+t[4]*d[1]+t[5]*d[2],t[6]*d[0]+t[7]*d[1]+t[8]*d[2]});}}
    bool moved=false;
    bool normalMoved=false;
    for(double t=0;t<lip.frames.size()/30.;t+=.1){
      assert(fo3face::BlendExpanded(bind,deltas,fo3face::Sample(lip,t),output));moved|=output!=bind;
      assert(fo3face::UpdateDirections(bind,mesh.indices,output));
      for(size_t v=0;v<mesh.indices.size();++v){
        for(size_t a=3;a<12;++a)assert(std::isfinite(output[v*18+a]));
        for(size_t a=3;a<6;++a)normalMoved|=std::fabs(output[v*18+a]-bind[v*18+a])>.001f;
        for(size_t a=12;a<18;++a)assert(output[v*18+a]==bind[v*18+a]);
      }
      assert(actor.position==root&&actor.yaw==.7f);
    }
    assert(moved);if(mesh.normals.size()==tri.vertices*3)assert(normalMoved);
    assert(fo3face::BlendExpanded(bind,deltas,{},output)&&fo3face::UpdateDirections(bind,mesh.indices,output)&&output==bind);
    std::cout<<"Original head NIF/TRI matched vertices="<<tri.vertices<<" indices="<<mesh.indices.size()<<" root preserved; neutral restored\n";
  }
  assert(matched==1);
}
