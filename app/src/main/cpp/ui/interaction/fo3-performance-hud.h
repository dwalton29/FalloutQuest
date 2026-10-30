#pragma once
#include "ui/fo3-system-colour.h"
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
    unsigned frames=0,synthesisFrames=0;
    bool spacewarp=false;
    double synthesisFps=0;
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
                 uint32_t residents,double displayHz,bool synthesisSubmitted) {
    auto& s=Get();
    if(mode==0) {s.frames=s.synthesisFrames=0;s.spacewarp=false;s.synthesisFps=0;s.cpuUs=0;s.started={};s.mode=0;return;}
    s.mode=mode;s.spacewarp=playable&&submitted&&synthesisSubmitted;
    const auto now=Clock::now();
    if(s.started.time_since_epoch().count()==0) s.started=now;
    if(playable&&submitted) {
        ++s.frames;if(synthesisSubmitted)++s.synthesisFrames;
        s.cpuUs+=std::max(0.0,workUs);
        s.residents=residents;
        s.hz=displayHz>0?int(std::lround(displayHz)):0;
    }
    const double elapsed=std::chrono::duration<double>(now-s.started).count();
    if(elapsed>=.5) {
        s.fps=elapsed>0 ? double(s.frames)/elapsed : 0;
        // Submission-based estimate, not a count of compositor-generated frames.
        s.synthesisFps=s.fps+double(s.synthesisFrames)/elapsed;
        if(s.hz>0)s.synthesisFps=std::min(s.synthesisFps,double(s.hz));
        s.cpuMs=s.frames ? s.cpuUs/(1000.0*s.frames) : 0;
        s.frames=s.synthesisFrames=0;s.cpuUs=0;s.started=now;
    }
}
inline void Prepare(unsigned mode) {
    if(!mode)return;
    auto& s=Get();
    fo3hudrenderer::GlStateGuard guard;
    if(!fo3hudrenderer::EnsureResources())return;
    const auto& font=fo3hudrenderer::State();
    char text[144]{};
    char warp[48]{},refresh[24]{};
    if(s.spacewarp)std::snprintf(warp,sizeof warp,"SW ~%.1f FPS",s.synthesisFps);
    else std::snprintf(warp,sizeof warp,"SW OFF");
    if(s.hz>0)std::snprintf(refresh,sizeof refresh,"%d Hz",s.hz);
    else std::snprintf(refresh,sizeof refresh,"Hz N/A");
    if(mode==1)std::snprintf(text,sizeof text,"APP %.1f FPS | %s | %s",s.fps,warp,refresh);
    else std::snprintf(text,sizeof text,
          "APP %.1f FPS | %s | %s\nCPU %.1fms   NPC %u   GPU N/A",
          s.fps,warp,refresh,s.cpuMs,s.residents);
    if(s.vao&&s.vbo&&mode==s.mode&&std::strcmp(s.text,text)==0)return;
    if(!s.vao) glGenVertexArrays(1,&s.vao);
    if(!s.vbo) glGenBuffers(1,&s.vbo);
    if(!s.vao||!s.vbo)return;
    // Head-relative plane at ~1.3m; no projection-space eye discrepancy.
    // The original Monofonto font texture is shared with authored HUD text.
    std::vector<fo3hudassets::Vertex> verts;
    verts.reserve(std::strlen(text)*6u);
    const float scale=.030f/std::max(1.f,font.baseLine);
    std::string_view remaining(text);float lineTop=0;
    while(!remaining.empty()){
    const auto newline=remaining.find('\n');
    fo3font::AppendText(font,remaining.substr(0,newline),0,lineTop,
        [&](uint8_t,const fo3font::Glyph& glyph,const fo3font::Quad& q) {
            fo3hudrenderer::AppendPromptQuad(verts,
                -.52f+q.left*scale,.28f-q.top*scale,
                -.52f+q.right*scale,.28f-q.bottom*scale,0,
                glyph.uv[0],glyph.uv[1],glyph.uv[2],glyph.uv[3],
                glyph.uv[4],glyph.uv[5],glyph.uv[6],glyph.uv[7]);
        });
    if(newline==std::string_view::npos)break;
    remaining.remove_prefix(newline+1);lineTop+=std::max(1.f,font.baseLine)*1.4f;
    }
    glBindVertexArray(s.vao);
    glBindBuffer(GL_ARRAY_BUFFER,s.vbo);
    glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(fo3hudassets::Vertex),
        verts.data(),GL_DYNAMIC_DRAW);
    fo3hudassets::UploadVertices(s.vao,verts.data(),verts.size());
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
    glUniform4f(font.tintLoc,fo3uicolor::Main[0],fo3uicolor::Main[1],fo3uicolor::Main[2],1.f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,font.fontTexture);
    glBindVertexArray(s.vao);
    glDrawArrays(GL_TRIANGLES,0,s.count);
}
} // namespace fo3perfui
