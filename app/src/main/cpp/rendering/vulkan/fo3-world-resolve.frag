#version 450
#extension GL_EXT_multiview : require
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
#ifdef FQ_SPLIT
layout(location=1) out vec4 base;
#endif
layout(std140,set=0,binding=0) uniform WorldRegion { vec4 rect; };
layout(set=0,binding=2) uniform sampler2DArray worldColor;
layout(set=0,binding=3) uniform sampler2DArray worldDepth;
void main() {
    vec2 pixels=clamp(rect.xy+uv*rect.zw,rect.xy+.5,rect.xy+rect.zw-.5);
    vec3 sampleUv=vec3(pixels/vec2(textureSize(worldColor,0).xy),float(gl_ViewIndex));
    color=textureLod(worldColor,sampleUv,0);
    gl_FragDepth=texelFetch(worldDepth,ivec3(ivec2(pixels),int(gl_ViewIndex)),0).r;
#ifdef FQ_SPLIT
    base=color;
#endif
}
