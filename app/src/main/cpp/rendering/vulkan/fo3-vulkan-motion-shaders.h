#pragma once
// Native Vulkan coordinates. History matrices retain the canonical GL camera;
// remap only raster coordinates and final motion, never FO3 world transforms.
namespace fqvk::motionShaders {
inline constexpr const char* static_vert=R"FQMV(#version 450
layout(location=0) out vec2 uv;
void main(){vec2 p=vec2(float((gl_VertexIndex<<1)&2),float(gl_VertexIndex&2));uv=p;gl_Position=vec4(p*2-1,0,1);}
)FQMV";
inline constexpr const char* static_frag=R"FQMV(#version 450
layout(std140,set=0,binding=0) uniform Motion {mat4 currentVP;mat4 previousVP;mat4 clipReprojection;mat4 model;mat4 previousModel;vec4 scaleEye;vec4 flags;vec4 target;};
layout(set=0,binding=2) uniform sampler2DArray sceneDepth;
layout(set=0,binding=3) uniform sampler2DArray dynamicMotion;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 velocity;
void main(){
 float d=texture(sceneDepth,vec3(uv,scaleEye.w)).r;gl_FragDepth=d;
 vec4 current=vec4(uv.x*2-1,1-uv.y*2,d*2-1,1);
 vec4 previous=clipReprojection*current;velocity=vec4(0);
 if(d<1&&previous.w>0.000001)velocity.xyz=(current.xyz-previous.xyz/previous.w)*scaleEye.xyz;
 // Match the exact full-resolution texel selected for retained scene depth.
 ivec2 pixel=clamp(ivec2(uv*vec2(textureSize(dynamicMotion,0).xy)),ivec2(0),textureSize(dynamicMotion,0).xy-1);
 vec4 objectMotion=texelFetch(dynamicMotion,ivec3(pixel,int(scaleEye.w)),0);
 if(objectMotion.w>.5)velocity.xyz=objectMotion.xyz;
}
)FQMV";
inline constexpr const char* dynamic_vert=R"FQMV(#version 450
layout(std140,set=0,binding=0) uniform Motion {mat4 currentVP;mat4 previousVP;mat4 clipReprojection;mat4 model;mat4 previousModel;vec4 scaleEye;vec4 flags;vec4 target;};
layout(set=0,binding=4) uniform sampler2D palette;
layout(set=0,binding=5) uniform sampler2D previousPalette;
layout(location=0) in vec3 position;
layout(location=4) in vec2 texcoord;
layout(location=5) in vec4 colour;
layout(location=10) in vec4 weights;
layout(location=11) in vec4 bones;
layout(location=12) in vec3 previousPosition;
layout(location=0) out vec4 currentClip;
layout(location=1) out vec4 previousClip;
layout(location=2) out vec2 uv;
layout(location=3) out float vertexAlpha;
mat4 bone(sampler2D p,int i){return mat4(texelFetch(p,ivec2(0,i),0),texelFetch(p,ivec2(1,i),0),texelFetch(p,ivec2(2,i),0),texelFetch(p,ivec2(3,i),0));}
vec3 skin(sampler2D p,vec3 v){
 if(flags.x==0)return v;
 mat4 m=mat4(0);float total=0;
 for(int i=0;i<4;++i)if(weights[i]>0){m+=bone(p,int(bones[i]))*weights[i];total+=weights[i];}
 if(flags.x==1){if(total<.999)m+=mat4(1)*max(0,1-total);}
 else {if(total>1e-8)m/=total;else m=mat4(1);}
 return (m*vec4(v,1)).xyz;
}
void main(){
 currentClip=currentVP*(model*vec4(skin(palette,position),1));
 previousClip=previousVP*(previousModel*vec4(skin(previousPalette,previousPosition),1));
 gl_Position=vec4(currentClip.x,-currentClip.y,(currentClip.z+currentClip.w)*.5,currentClip.w);
 uv=texcoord;vertexAlpha=colour.a;
}
)FQMV";
inline constexpr const char* dynamic_frag=R"FQMV(#version 450
layout(std140,set=0,binding=0) uniform Motion {mat4 currentVP;mat4 previousVP;mat4 clipReprojection;mat4 model;mat4 previousModel;vec4 scaleEye;vec4 flags;vec4 target;};
layout(set=0,binding=2) uniform sampler2DArray sceneDepth;
layout(set=0,binding=3) uniform sampler2D diffuse;
layout(location=0) in vec4 currentClip;
layout(location=1) in vec4 previousClip;
layout(location=2) in vec2 uv;
layout(location=3) in float vertexAlpha;
layout(location=0) out vec4 velocity;
void main(){
 vec2 screen=gl_FragCoord.xy/target.xy;
 vec2 sourceSize=vec2(textureSize(sceneDepth,0).xy);
 vec2 sampleUv=(floor(screen*sourceSize)+.5)/sourceSize;
 vec2 offset=(sampleUv-screen)*target.xy;
 float d=gl_FragCoord.z;
 float sampleDepth=d+dFdx(d)*offset.x+dFdy(d)*offset.y;
 if(sampleDepth>texture(sceneDepth,vec3(screen,scaleEye.w)).r+2.0/16777215.0)discard;
 if(flags.y>0&&texture(diffuse,uv).a*flags.w*(flags.z>0?vertexAlpha:1)<target.z)discard;
 velocity=vec4(0,0,0,1);
 if(previousClip.w>0.000001&&currentClip.w>0.000001)velocity.xyz=(currentClip.xyz/currentClip.w-previousClip.xyz/previousClip.w)*scaleEye.xyz;
}
)FQMV";
}
