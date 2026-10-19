#pragma once
// Q22.7: independent, optional diagnostic HUD. One mono font geometry build
// per half-second, then a single small stereo draw per eye. Rendering text is
// kept outside the world/Pip-Boy texture and all Fallout save authority.
#include "ui/interaction/fo3-interaction-hud-renderer.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace fo3perfui {
using Clock=std::chrono::steady_clock;
struct State {
    GLuint vao=0,vbo=0;
    GLsizei count=0;
    char text[144]{};
    unsigned mode=0;
    unsigned frames=0;
    double cpuUs=0;
    uint32_t residents=0;
    int hz=72;
    double fps=0,cpuMs=0;
    Clock::time_point started{};
};
inline State& Get(){static State s;return s;}
inline void Shutdown() {
    auto& s=Get();
    if(s.vbo)glDeleteBuffers(1,&s.vbo);
    if(s.vao)glDeleteVertexArrays(1,&s.vao);
    s={};
}
inline void Tick(unsigned mode,bool submitted,bool playable,double workUs,
                 uint32_t residents,double periodUs) {
    auto& s=Get();
    if(mode==0) {s.frames=0;s.cpuUs=0;s.started={};s.mode=0;return;}
    s.mode=mode;
    const auto now=Clock::now();
    if(s.started.time_since_epoch().count()==0) s.started=now;
    if(playable&&submitted) {
        ++s.frames;
        s.cpuUs+=std::max(0.0,workUs);
        s.residents=residents;
        if(periodUs>1000) s.hz=int(std::lround(1000000.0/periodUs));
    }
    const double elapsed=std::chrono::duration<double>(now-s.started).count();
    if(elapsed>=.5) {
        s.fps=elapsed>0 ? double(s.frames)/elapsed : 0;
        s.cpuMs=s.frames ? s.cpuUs/(1000.0*s.frames) : 0;
        s.frames=0;s.cpuUs=0;s.started=now;
    }
}
inline void Prepare(unsigned mode) {
    if(!mode)return;
    auto& s=Get();
    fo3hudrenderer::GlStateGuard guard;
    if(!fo3hudrenderer::EnsureResources())return;
    const auto& font=fo3hudrenderer::State();
    char text[144]{};
    if(mode==1) std::snprintf(text,sizeof text,"APP %.1f FPS / %d Hz",s.fps,s.hz);
    else std::snprintf(text,sizeof text,
          "APP %.1f FPS / %d Hz   CPU %.1fms   NPC %u   GPU N/A",
          s.fps,s.hz,s.cpuMs,s.residents);
    if(s.vao&&s.vbo&&mode==s.mode&&std::strcmp(s.text,text)==0)return;
    if(!s.vao) glGenVertexArrays(1,&s.vao);
    if(!s.vbo) glGenBuffers(1,&s.vbo);
    if(!s.vao||!s.vbo)return;
    // Head-relative plane at ~1.3m; no projection-space eye discrepancy.
    // The original Monofonto font texture is shared with authored HUD text.
    std::vector<fo3hudassets::Vertex> verts;
    verts.reserve(std::strlen(text)*6u);
    const float scale=.030f/std::max(1.f,font.baseLine);
    fo3font::AppendText(font,std::string_view(text),0,0,
        [&](uint8_t,const fo3font::Glyph& glyph,const fo3font::Quad& q) {
            fo3hudrenderer::AppendPromptQuad(verts,
                -.52f+q.left*scale,.28f-q.top*scale,
                -.52f+q.right*scale,.28f-q.bottom*scale,0,
                glyph.uv[0],glyph.uv[1],glyph.uv[2],glyph.uv[3],
                glyph.uv[4],glyph.uv[5],glyph.uv[6],glyph.uv[7]);
        });
    glBindVertexArray(s.vao);
    glBindBuffer(GL_ARRAY_BUFFER,s.vbo);
    glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(fo3hudassets::Vertex),
        verts.data(),GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),
                          reinterpret_cast<const void*>(3*sizeof(float)));
    s.count=GLsizei(verts.size());
    std::strncpy(s.text,text,sizeof(s.text)-1);
    s.mode=mode;
}
inline void Render(const float* mvp,unsigned mode){
    const auto& s=Get();
    const auto& font=fo3hudrenderer::State();
    if(!mode||!s.count||!font.ready||!mvp)return;
    fo3hudrenderer::GlStateGuard guard;
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,
        GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(font.program);
    glUniformMatrix4fv(font.mvpLoc,1,GL_FALSE,mvp);
    glUniform1i(font.texLoc,0);
    glUniform4f(font.tintLoc,26.f/255.f,1.f,128.f/255.f,1.f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,font.fontTexture);
    glBindVertexArray(s.vao);
    glDrawArrays(GL_TRIANGLES,0,s.count);
}
} // namespace fo3perfui
