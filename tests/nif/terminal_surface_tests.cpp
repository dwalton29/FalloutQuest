#include <fstream>
#include <iostream>
#include <cassert>
#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include "ui/terminal/fo3-terminal-surface.h"
bool LoadFalloutMeshFile(const std::string& path,std::vector<uint8_t>& bytes,std::string*){std::ifstream f(path,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(f),{});return !bytes.empty();}
int main(int argc,char** argv){
 for(int a=1;a<argc;++a){std::vector<Fo3StaticNifMesh> meshes;assert(LoadFo3StaticNifMeshes(argv[a],meshes,false,true));
  fo3terminal::Surface largest;std::string name;
  for(const auto& m:meshes){auto tex=m.diffuseTexturePath;for(auto& c:tex)c=char(std::tolower((unsigned char)c));
   if(tex.find("screen")==std::string::npos||tex.find("glare")!=std::string::npos)continue;
   std::vector<fo3terminal::Point> p,n;
   for(size_t i=0;i<m.positions.size();i+=3){p.push_back({m.positions[i]/70,m.positions[i+2]/70,-m.positions[i+1]/70});n.push_back({m.normals[i],m.normals[i+2],-m.normals[i+1]});}
   fo3terminal::Surface s;if(s.Build(p,m.indices,n)&&s.width*s.height>largest.width*largest.height){largest=std::move(s);name=m.shapeName;}
  }
  assert(largest.Ready()&&largest.width>.2f&&largest.height>.2f);
  assert(fo3terminal::Dot(largest.normal,largest.normal)>.99f);
  for(float y:{.1f,.3f,.5f,.7f,.9f})for(float x:{.1f,.3f,.5f,.7f,.9f}){
   auto point=largest.At(x,y);auto d=fo3terminal::Sub(point,largest.origin);
   assert(std::fabs((fo3terminal::Dot(d,largest.right)-largest.left)/largest.width-x)<1e-4f);
   assert(std::fabs((fo3terminal::Dot(d,largest.up)-largest.bottom)/largest.height-(1-y))<1e-4f);
  }
  std::cout<<argv[a]<<" screen="<<name<<" size="<<largest.width<<","<<largest.height<<" normal="<<largest.normal[0]<<","<<largest.normal[1]<<","<<largest.normal[2]<<"\n";
 }
}
