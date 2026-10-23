#pragma once
#include "fo3-interaction-hud-renderer.h"
#include "fo3-item-notifications.h"
#include "fo3-hud-cached-state.h"
#include <unordered_map>

namespace fo3notifyui {
struct Resources {
    GLuint vao=0,vbo=0,iconTexture=0,program=0;
    GLint mvp=-1,tint=-1,tex=-1,age=-1;
    std::unordered_map<std::string,GLuint> icons;
    GLsizei iconFirst=0;
    GLsizei count=0;
    uint64_t revision=UINT64_MAX;
};
inline Resources& State() {static Resources r;return r;}
struct AnimatedVertex {fo3hudassets::Vertex glyph;float delay=0;};
inline GLuint BuildAnimatedProgram() {
    const char* vs=R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUv;
layout(location=2) in float aDelay;
uniform mat4 uMvp;
out vec2 vUv;
out float vDelay;
void main(){gl_Position=uMvp*vec4(aPos,1.0);vUv=aUv;vDelay=aDelay;})";
    const char* fs=R"(#version 300 es
precision mediump float;
in vec2 vUv;
in float vDelay;
uniform sampler2D uTex;
uniform vec4 uTint;
uniform float uAge;
out vec4 fragColor;
void main(){
    vec4 texel=texture(uTex,vUv);
    float fade=uAge<0.0?1.0:clamp(min((uAge-vDelay)/2.0,(vDelay+8.0-uAge)/2.0),0.0,1.0);
    fragColor=vec4(texel.rgb*uTint.rgb,texel.a*uTint.a*fade);
})";
    const GLuint v=fo3hudassets::CompileShader(GL_VERTEX_SHADER,vs),f=fo3hudassets::CompileShader(GL_FRAGMENT_SHADER,fs);
    if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return 0;}
    const GLuint program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);
    glDeleteShader(v);glDeleteShader(f);GLint ok=GL_FALSE;glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok){glDeleteProgram(program);return 0;}
#ifdef FQ_ENABLE_VULKAN
    fqvk::Program(program,10);
#endif
    return program;
}
inline void Shutdown() {
    auto& r=State();
    if(r.vao)glDeleteVertexArrays(1,&r.vao);
    if(r.vbo)glDeleteBuffers(1,&r.vbo);
    if(r.program)glDeleteProgram(r.program);
    for(const auto& icon:r.icons)if(icon.second)glDeleteTextures(1,&icon.second);
    r={};fo3notify::Notifications().Clear();
}
// Once per logical frame, outside stereo eyes. Existing font assets are warmed
// before any notification can be rendered; no asset access in Render().
inline void Prepare(double now,bool available,bool sceneReady) {
    auto& queue=fo3notify::Notifications();
    // Warm assets once a scene has established install paths, including while
    // a transition overlay is present. Never load assets in a stereo draw.
    fo3hudrenderer::CachedStateGuard guard;
    if(sceneReady && !fo3hudrenderer::State().attempted) fo3hudrenderer::EnsureResources();
    queue.Advance(now,available);
    if(!available)return;
    if(!fo3hudrenderer::State().ready) return;
    auto& r=State();auto& font=fo3hudrenderer::State();
    if(!r.vao) {
        glGenVertexArrays(1,&r.vao);glGenBuffers(1,&r.vbo);r.program=BuildAnimatedProgram();
        r.mvp=glGetUniformLocation(r.program,"uMvp");r.tint=glGetUniformLocation(r.program,"uTint");
        r.tex=glGetUniformLocation(r.program,"uTex");r.age=glGetUniformLocation(r.program,"uAge");
    }
    if(r.revision==queue.Revision()) return;
    r.revision=queue.Revision();r.count=0;r.iconTexture=0;
    if(!queue.Icon().empty()){
        auto found=r.icons.find(queue.Icon());
        if(found==r.icons.end()){
            Fo3RgbaTexture texture;GLuint handle=0;
            if(LoadFalloutTextureRgba(queue.Icon(),texture))handle=fo3hudassets::UploadTexture(texture.width,texture.height,texture.rgba.data());
            found=r.icons.emplace(queue.Icon(),handle).first;
            if(!handle)__android_log_print(ANDROID_LOG_WARN,"FalloutQuest","KARMA HUD missing original icon: %s",queue.Icon().c_str());
        }
        r.iconTexture=found->second;
    }
    const std::string_view text(queue.Text());
    if(text.empty())return;
    std::vector<fo3hudassets::Vertex> vertices;vertices.reserve(text.size()*6);
    // HUDTemplates left-justified font 7. 460px Messages width, two rows within
    // its 90px height. Pixel->metre size and upper-right placement are VR-only.
    constexpr float scale=.00125f;
    const bool quest=queue.Quest();const float wrap=quest?400.f:460.f;
    const float left=quest?-.70f:(r.iconTexture?.30f:.22f),top=quest?.32f:.43f;
    size_t begin=0;int line=0;
    while(begin<text.size() && line<(quest?4:2)) {
        const auto newline=text.find('\n',begin);
        const auto row=text.substr(begin,newline==std::string_view::npos?text.size()-begin:newline-begin);
        size_t n=fo3font::FitText(font,row,wrap);
        if(!n)break;
        if(n<row.size()) {
            const auto space=text.substr(begin,n).find_last_of(' ');
            if(space!=std::string_view::npos && space>0)n=space;
        }
        fo3font::AppendText(font,text.substr(begin,n),0,0,
            [&](uint8_t,const fo3font::Glyph& g,const fo3font::Quad& q) {
                fo3hudrenderer::AppendPromptQuad(vertices,left+q.left*scale,top-(q.top+line*45)*scale,
                    left+q.right*scale,top-(q.bottom+line*45)*scale,-2.0f,
                    g.uv[0],g.uv[1],g.uv[2],g.uv[3],g.uv[4],g.uv[5],g.uv[6],g.uv[7]);
            });
        begin+=n;while(begin<text.size() && (text[begin]==' '||text[begin]=='\n'))++begin;++line;
    }
    r.count=static_cast<GLsizei>(vertices.size());r.iconFirst=r.count;
    if(r.iconTexture)fo3hudrenderer::AppendPromptQuad(vertices,.22f,.43f,.29f,.36f,-2.f,0,0,1,0,0,1,1,1);
    std::vector<AnimatedVertex> animated;animated.reserve(vertices.size());
    for(size_t i=0;i<vertices.size();++i)animated.push_back({vertices[i],quest?float(i/6)*.1f:0.f});
    glBindVertexArray(r.vao);glBindBuffer(GL_ARRAY_BUFFER,r.vbo);
    glBufferData(GL_ARRAY_BUFFER,animated.size()*sizeof(AnimatedVertex),animated.data(),GL_DYNAMIC_DRAW);
#ifdef FQ_ENABLE_VULKAN
    static_assert(sizeof(AnimatedVertex)==6*sizeof(float));
    fqvk::Mesh(r.vao,reinterpret_cast<const float*>(animated.data()),animated.size(),6,nullptr,0);
#endif
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(AnimatedVertex),nullptr);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(AnimatedVertex),reinterpret_cast<void*>(3*sizeof(float)));
    glEnableVertexAttribArray(2);glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,sizeof(AnimatedVertex),reinterpret_cast<void*>(5*sizeof(float)));
}
inline void Render(const float* mvp) {
    const auto& r=State();const auto& font=fo3hudrenderer::State();
    const float alpha=fo3notify::Notifications().Alpha();
    if(!font.ready || !r.program || !r.count || alpha<=0)return;
    const auto queries=fqgl::counters.queries;
    {
        fo3hudrenderer::CachedStateGuard guard;
        glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(r.program);glUniformMatrix4fv(r.mvp,1,GL_FALSE,mvp);
        glUniform4f(r.tint,26.0f/255,1,128.0f/255,fo3notify::Notifications().Quest()?1.f:alpha);glUniform1i(r.tex,0);
        glUniform1f(r.age,fo3notify::Notifications().Quest()?float(fo3notify::Notifications().Age()):-1.f);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,font.fontTexture);
        glBindVertexArray(r.vao);glDrawArrays(GL_TRIANGLES,0,r.count);
        if(r.iconTexture){glBindTexture(GL_TEXTURE_2D,r.iconTexture);glDrawArrays(GL_TRIANGLES,r.iconFirst,6);}
    }
    if(fqgl::counters.queries!=queries)
        __android_log_print(ANDROID_LOG_ERROR,"FalloutQuest","ITEM ADDED HUD: unexpected driver query");
}
} // namespace fo3notifyui
