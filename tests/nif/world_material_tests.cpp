#include <cassert>
#include <fstream>
#include <iostream>
#include "rendering/mesh/fo3-lod-material.h"
#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
bool LoadFalloutMeshFile(const std::string& path,std::vector<uint8_t>& bytes,std::string* resolved){
 std::ifstream f(path,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(f),{});if(resolved)*resolved=path;return !bytes.empty();
}
int main(int argc,char**argv){
 assert(fo3lod::Surface(true,2)&&fo3lod::Surface(true,4));assert(!fo3lod::Surface(false,4)&&!fo3lod::Surface(true,1));
 assert(fo3lod::Cutout(true,4,false,true,false));
 assert(!fo3lod::Cutout(true,4,true,true,false)); // Explicit disabled alpha is authored too.
 assert(!fo3lod::Cutout(true,4,false,true,true));
 assert(!fo3lod::Cutout(false,4,false,true,false));
 assert(!fo3lod::Cutout(true,2,false,true,false)); // LAND alpha is not coverage.
 assert(!fo3lod::Cutout(true,4,false,false,false));
 NifHeader h;h.userVersion=11;h.bsVersion=34;h.numBlocks=3;h.blockTypes={"NiMaterialProperty","BSShaderPPLightingProperty","NiTexturingProperty"};h.blockTypeIndices={0,1,2};
 ShapeObject shape;shape.properties={0};assert(MaterialOnlyWorldShape(shape,h));
 shape.properties={0,1};assert(!MaterialOnlyWorldShape(shape,h));
 shape.properties={0,2};assert(!MaterialOnlyWorldShape(shape,h));
 shape.properties.clear();assert(!MaterialOnlyWorldShape(shape,h));
 // A material-only child can inherit a real shader and alpha state.
 NifHeader inherited;inherited.userVersion=11;inherited.bsVersion=34;inherited.numBlocks=5;
 inherited.blockTypes={"NiNode","NiMaterialProperty","BSShaderPPLightingProperty","NiAlphaProperty","NiTriShape"};inherited.blockTypeIndices={0,1,2,3,4};
 std::vector<uint8_t> node;auto word=[&](uint32_t v){for(int i=0;i<4;++i)node.push_back(uint8_t(v>>(8*i)));};
 word(INVALID_REF);word(0);word(INVALID_REF);word(0);for(int i=0;i<13;++i)word(0);
 word(2);word(2);word(3);word(INVALID_REF);
 inherited.blockOffsets={0,0,0,0,0};inherited.blockSizes={uint32_t(node.size()),0,0,0,0};
 ShapeObject child;child.properties={1};InheritShapeProperties(node,inherited,{INVALID_REF,INVALID_REF,INVALID_REF,INVALID_REF,0},4,child);
 assert(child.properties==std::vector<uint32_t>({1,2,3})&&!MaterialOnlyWorldShape(child,inherited));
 InheritShapeProperties(node,inherited,{INVALID_REF,INVALID_REF,INVALID_REF,INVALID_REF,0},4,child);
 assert(child.properties==std::vector<uint32_t>({1,2,3}));
 if(argc>1){
  std::vector<Fo3StaticNifMesh> standalone,world;
  assert(LoadFo3StaticNifMeshes(argv[1],standalone,true));assert(LoadFo3StaticNifMeshes(argv[1],world,true,true));
  assert(standalone.size()==11&&world.size()==10);
  for(const auto& m:world)assert(m.shapeBlock!=8&&!m.diffuseTexturePath.empty());
  assert(std::count_if(standalone.begin(),standalone.end(),[](const auto& m){return m.shapeBlock==8&&m.indices.size()==86*3&&m.diffuseTexturePath.empty();})==1);
  std::cout<<"Original Mart: shaderless 86-triangle shape omitted, all ten real surfaces preserved\n";
 }
 if(argc>2){std::vector<Fo3StaticNifMesh> lod;assert(LoadFo3StaticNifMeshes(argv[2],lod,true));assert(lod.size()==1);
  assert(!lod[0].alphaPropertyPresent&&!lod[0].alphaTest&&!lod[0].alphaBlend);
  assert(fo3lod::Cutout(true,lod[0].shaderFlags2,lod[0].alphaPropertyPresent,true,false));
  std::cout<<"Original native building LOD admits transparent atlas cutout without ordinary alpha property\n";
 }
 std::cout<<"World material and native LOD admission passed\n";
}
