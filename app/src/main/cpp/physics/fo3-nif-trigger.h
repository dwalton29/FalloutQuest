#pragma once
#include "fo3-nif-collision.h"
#include "pipboy/fo3-pipboy-data.h"

namespace fo3trigger {
// Havok stores three basis vectors and one translation in padded vector4s.
// The padding is uninitialised in some shipped files.
inline bool ReadTransform(const uint8_t* data,fo3_q6f_collision_detail::Mat4& out){
  using namespace fo3_q6f_collision_detail;
  out=Identity();
  for(unsigned col=0;col<4;++col)for(unsigned row=0;row<3;++row){
    float value=ReadLeFloat(data+16*col+4*row);
    if(!std::isfinite(value))return false;
    out.m[4*row+col]=col==3?value*FO3_HAVOK_TO_NIF:value;
  }
  return true;
}
inline bool DecodePhantom(const std::vector<uint8_t>& bytes,fo3pipdata::TriggerPrimitive& out){
  using namespace fo3_q6f_collision_detail;
  NifHeader header;if(!ParseHeader(bytes,header))return false;
  fo3pipdata::TriggerPrimitive shape;
  for(uint32_t i=0;i<header.numBlocks;++i){
    if(BlockType(header,i)!="bhkSPCollisionObject")continue;
    // A placed reference supplies the root transform. Child-node attachments
    // and compound phantoms need their own transforms and union of shapes.
    const auto* object=BlockData(bytes,header,i);
    if(shape.valid||!object||header.blockSizes[i]!=10||ReadLe32(object)!=0)return false;
    const auto body=ReadLe32(object+6);
    if(body>=header.numBlocks||BlockType(header,body)!="bhkSimpleShapePhantom"||header.blockSizes[body]!=100)return false;
    const auto* phantom=BlockData(bytes,header,body);
    Mat4 transform;if(!ReadTransform(phantom+36,transform))return false;
    auto leaf=ReadLe32(phantom);
    for(unsigned depth=0;leaf<header.numBlocks&&depth<32;++depth){
      const auto& t=BlockType(header,leaf);
      if(t!="bhkTransformShape"&&t!="bhkConvexTransformShape")break;
      const auto* wrapper=BlockData(bytes,header,leaf);
      Mat4 local;if(header.blockSizes[leaf]!=84||!ReadTransform(wrapper+20,local))return false;
      transform=Mul(transform,local);leaf=ReadLe32(wrapper);
    }
    if(leaf>=header.numBlocks||BlockType(header,leaf)!="bhkBoxShape"||header.blockSizes[leaf]!=32)return false;
    const auto* box=BlockData(bytes,header,leaf);
    for(unsigned axis=0;axis<3;++axis){
      shape.center[axis]=transform.m[4*axis+3];
      shape.bounds[axis]=ReadLeFloat(box+16+4*axis)*FO3_HAVOK_TO_NIF;
      if(!std::isfinite(shape.bounds[axis])||shape.bounds[axis]<=0||!std::isfinite(shape.center[axis]))return false;
      for(unsigned r=0;r<3;++r)shape.axes[3*axis+r]=transform.m[4*r+axis];
    }
    for(unsigned a=0;a<3;++a)for(unsigned b=0;b<=a;++b){
      float dot=0;for(unsigned r=0;r<3;++r)dot+=shape.axes[3*a+r]*shape.axes[3*b+r];
      if(!std::isfinite(dot)||std::abs(dot-(a==b?1.f:0.f))>.001f)return false;
    }
    shape.kind=1;shape.modelSpace=true;shape.valid=true;
  }
  if(!shape.valid)return false;
  out=shape;return true;
}
}
