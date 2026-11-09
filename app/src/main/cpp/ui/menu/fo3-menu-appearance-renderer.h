#pragma once

#include "fo3-menu-renderer.h"
#include "rendering/mesh/fo3-static-nif.h"

namespace fo3menuui {
struct GeneProjectionFrame {
  Surface casing, face;
  std::array<Surface,4> lights;
  fo3menu::Box view{}, aperture{};
  std::vector<fo3hudassets::Vertex> apertureTriangles;
  std::array<float,4> portraitUv{};
  bool attempted=false, ready=false;

  static fo3menu::Box Bounds(const Fo3StaticNifMesh& mesh) {
    float left=INFINITY, top=INFINITY, right=-INFINITY, bottom=-INFINITY;
    for(size_t i=0;i+2<mesh.positions.size();i+=3){
      const float x=mesh.positions[i], y=-mesh.positions[i+2];
      left=std::min(left,x); right=std::max(right,x);
      top=std::min(top,y); bottom=std::max(bottom,y);
    }
    return {left,top,right-left,bottom-top};
  }

  bool Ensure(const fo3menu::Box& lcd) {
    if(attempted)return ready;
    attempted=true;
    std::vector<Fo3StaticNifMesh> meshes;
    if(!Shared().Ensure()||!LoadFo3StaticNifMeshes("Terminals\\RaceSexInterface01.NIF",meshes))
      return false;
    const auto find=[&](const char* name)->const Fo3StaticNifMesh*{
      const auto it=std::find_if(meshes.begin(),meshes.end(),[&](const auto& mesh){return mesh.shapeName==name;});
      return it==meshes.end()?nullptr:&*it;
    };
    const auto* shell=find("RaceSexInterface01:0");
    const auto* small=find("SmallScreenGlare:0");
    const auto* big=find("BigScreenGlare:0");
    if(!shell||!small||!big)return false;
    const auto screen=Bounds(*small);
    if(!(screen.w>0&&screen.h>0&&lcd.w>0&&lcd.h>0))return false;
    const float sx=lcd.w/screen.w, sy=lcd.h/screen.h;
    const auto project=[&](float x,float y){
      return std::array<float,2>{lcd.x+(x-screen.x)*sx,lcd.y+(y-screen.y)*sy};
    };
    const auto projectBox=[&](const fo3menu::Box& box){
      const auto origin=project(box.x,box.y);
      return fo3menu::Box{origin[0],origin[1],box.w*sx,box.h*sy};
    };
    view=projectBox(Bounds(*shell));
    aperture=projectBox(Bounds(*big));
    if(!(aperture.w>0&&aperture.h>0&&view.w>0&&view.h>0))return false;

    struct Triangle {
      std::array<fo3hudassets::Vertex,3> vertices;
      float depth=0;
      GLuint texture=0;
    };
    const auto collect=[&](const Fo3StaticNifMesh& mesh,GLuint texture,std::vector<Triangle>& out){
      const size_t count=mesh.positions.size()/3;
      if(mesh.texcoords.size()!=count*2)return;
      for(size_t i=0;i+2<mesh.indices.size();i+=3){
        Triangle triangle;
        triangle.texture=texture;
        bool valid=true;
        for(size_t corner=0;corner<3;++corner){
          const size_t j=mesh.indices[i+corner];
          if(j>=count){valid=false;break;}
          const auto xy=project(mesh.positions[j*3],-mesh.positions[j*3+2]);
          triangle.vertices[corner]={xy[0],xy[1],0,mesh.texcoords[j*2],mesh.texcoords[j*2+1]};
          triangle.depth+=mesh.positions[j*3+1];
        }
        if(!valid)continue;
        const auto& [a,b,c]=triangle.vertices;
        // The authored front faces -Y in NIF space.
        if((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)<0)
          out.push_back(triangle);
      }
    };
    const auto upload=[](Surface& surface,std::vector<Triangle>& triangles){
      std::stable_sort(triangles.begin(),triangles.end(),[](const auto& a,const auto& b){return a.depth>b.depth;});
      for(const auto& triangle:triangles){
        if(surface.batches.empty()||surface.batches.back().texture!=triangle.texture){
          Batch batch;
          std::fill_n(batch.rgb,3,1.f);
          batch.texture=triangle.texture;
          batch.first=GLsizei(surface.vertices.size());
          surface.batches.push_back(batch);
        }
        surface.batches.back().count+=3;
        surface.vertices.insert(surface.vertices.end(),triangle.vertices.begin(),triangle.vertices.end());
      }
      if(!surface.vertices.empty())surface.Upload();
    };

    // The retail NIF names the Xbox material. Prefer the supplied PC art.
    auto art=Shared().Image("textures\\terminals\\pc\\english\\RaceSexInterface01.dds",{}).id;
    if(!art)art=Shared().Image("textures\\terminals\\RaceSexInterface01.dds",{}).id;
    if(!art)art=Shared().Image(shell->diffuseTexturePath,{}).id;
    if(!art)return false;
    std::vector<Triangle> triangles;
    for(const auto& mesh:meshes){
      if(mesh.diffuseTexturePath==shell->diffuseTexturePath)collect(mesh,art,triangles);
      else if(mesh.shapeName=="Screen:0"){
        const auto texture=Shared().Image(mesh.diffuseTexturePath,{}).id;
        if(texture)collect(mesh,texture,triangles);
      }
    }
    upload(casing,triangles);
    if(casing.vertices.empty())return false;
    const char* const stages[]={"SexGlow:0","RaceGlow:0","FaceGlow:0","BodyGlow:0"};
    for(size_t i=0;i<lights.size();++i)if(const auto* light=find(stages[i])){
      const auto texture=Shared().Image(light->diffuseTexturePath,{}).id;
      if(!texture)continue;
      triangles.clear();
      collect(*light,texture,triangles);
      upload(lights[i],triangles);
    }
    triangles.clear();
    collect(*big,0,triangles);
    for(const auto& triangle:triangles)
      apertureTriangles.insert(apertureTriangles.end(),triangle.vertices.begin(),triangle.vertices.end());
    ready=!apertureTriangles.empty();
    return ready;
  }

  void Draw(const float* mvp,Surface& layout,unsigned stage,GLuint portrait,
            const std::array<float,4>& uv) {
    const float scale=1.28f/view.w;
    WorldRegion(casing,mvp,view,scale);
    if(portrait){
      if(face.vertices.empty()||portraitUv!=uv){
        face.vertices=apertureTriangles;
        for(auto& vertex:face.vertices){
          vertex.u=uv[0]+(uv[2]-uv[0])*(vertex.x-aperture.x)/aperture.w;
          vertex.v=uv[1]+(uv[3]-uv[1])*(vertex.y-aperture.y)/aperture.h;
        }
        face.batches.resize(1);
        std::fill_n(face.batches.front().rgb,3,1.f);
        face.batches.front().count=GLsizei(face.vertices.size());
        face.Upload();
        portraitUv=uv;
      }
      face.batches.front().texture=portrait;
      WorldRegion(face,mvp,view,scale);
    }
    WorldRegion(lights[stage],mvp,view,scale);
    WorldRegion(layout,mvp,view,scale);
  }

  void Shutdown() {
    casing.Shutdown();
    face.Shutdown();
    for(auto& light:lights)light.Shutdown();
    *this={};
  }
};

inline GeneProjectionFrame& AppearanceFrame(){
  static GeneProjectionFrame frame;
  return frame;
}
}
