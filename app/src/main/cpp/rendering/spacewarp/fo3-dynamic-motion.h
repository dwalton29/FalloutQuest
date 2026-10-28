#pragma once
namespace fqspacewarp {
inline constexpr const char* DynamicVertex=R"GLSL(#version 300 es
precision highp float;
precision highp sampler2D;
layout(location=0) in vec3 aPosition;
layout(location=4) in vec2 aUv;
layout(location=5) in vec4 aColor;
layout(location=10) in vec4 aWeights;
layout(location=11) in vec4 aBones;
layout(location=12) in vec3 aPreviousPosition;
uniform mat4 uCurrentVP,uPreviousVP,uModel,uPreviousModel;
uniform sampler2D uPalette,uPreviousPalette;
uniform int uSkinMode;
out vec4 currentClip,previousClip;
out vec2 uv;
out float vertexAlpha;
mat4 bone(sampler2D palette,int index){
 return mat4(texelFetch(palette,ivec2(0,index),0),texelFetch(palette,ivec2(1,index),0),
             texelFetch(palette,ivec2(2,index),0),texelFetch(palette,ivec2(3,index),0));
}
vec3 skin(sampler2D palette,vec3 position){
 if(uSkinMode==0)return position;
 mat4 m=mat4(0.0);float total=0.0;
 for(int i=0;i<4;++i)if(aWeights[i]>0.0){m+=bone(palette,int(aBones[i]))*aWeights[i];total+=aWeights[i];}
 if(uSkinMode==1){if(total<0.999)m+=mat4(1.0)*max(0.0,1.0-total);}
 else {if(total>1e-8)m/=total;else m=mat4(1.0);}
 return (m*vec4(position,1.0)).xyz;
}
void main(){
 currentClip=uCurrentVP*uModel*vec4(skin(uPalette,aPosition),1.0);
 previousClip=uPreviousVP*uPreviousModel*vec4(skin(uPreviousPalette,aPreviousPosition),1.0);
 gl_Position=currentClip;uv=aUv;vertexAlpha=aColor.a;
}
)GLSL";
inline constexpr const char* DynamicFragment=R"GLSL(#version 300 es
precision highp float;
in vec4 currentClip,previousClip;
in vec2 uv;
in float vertexAlpha;
uniform sampler2D uDiffuse,uSceneDepth;
uniform vec2 uTargetSize;
uniform vec3 uMotionScale;
uniform bool uAlphaTest,uVertexAlpha;
uniform float uAlpha,uThreshold;
layout(location=0) out vec4 velocity;
void main(){
 // Compare at the source depth texel's actual sample location. A direct
 // low-resolution depth test rejects visible slopes when resolutions differ.
 vec2 screenUv=gl_FragCoord.xy/uTargetSize;
 vec2 sourceSize=vec2(textureSize(uSceneDepth,0));
 vec2 sampleUv=(floor(screenUv*sourceSize)+.5)/sourceSize;
 vec2 offset=(sampleUv-screenUv)*uTargetSize;
 float depth=gl_FragCoord.z;
 float sampleDepth=depth+dFdx(depth)*offset.x+dFdy(depth)*offset.y;
 if(sampleDepth>texture(uSceneDepth,screenUv).r+2.0/16777215.0)discard;
 if(uAlphaTest&&texture(uDiffuse,uv).a*uAlpha*(uVertexAlpha?vertexAlpha:1.0)<uThreshold)discard;
 velocity=vec4(0.0);
 if(previousClip.w>0.000001&&currentClip.w>0.000001)
   velocity.xyz=(currentClip.xyz/currentClip.w-previousClip.xyz/previousClip.w)*uMotionScale;
}
)GLSL";
} // namespace fqspacewarp
