#pragma once
// Vanilla HUDMainMenu/XPMeter. Source nodes: XPBracket, XPPointer,
// XPAmount, XPLabel, XPLastLevel, XPNextLevel and XPLevelUp.
// Original TAI sprite coordinates and original Fallout font are loaded from
// the installed game. The pointer's slide is the Quest adaptation of a live
// change in XP, evaluated once per logical frame, never independently per eye.
#include "ui/interaction/fo3-item-notifications.h"
#include "ui/interaction/fo3-interaction-hud-renderer.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace fo3xpui {
struct Resources {
    bool attempted=false,ready=false,visible=false;
    GLuint vao=0,vbo=0,bracket=0,pointer=0;
    fo3hudassets::TaiSprite bracketSprite{},pointerSprite{};
    uint64_t revision=UINT64_MAX;
    GLsizei bracketFirst=0,bracketCount=0,pointerFirst=0,pointerCount=0;
    GLsizei textFirst=0,textCount=0;
    float priorFraction=0,finalFraction=0,slide=0;
};
inline Resources& State(){static Resources s;return s;}
inline void Shutdown(){
    auto& s=State();
    if(s.vbo)glDeleteBuffers(1,&s.vbo);
    if(s.vao)glDeleteVertexArrays(1,&s.vao);
    if(s.bracket)glDeleteTextures(1,&s.bracket);
    if(s.pointer&&s.pointer!=s.bracket)glDeleteTextures(1,&s.pointer);
    s={};
}
inline GLuint OriginalAtlas(const std::string& path){
    Fo3RgbaTexture art;
    if(!LoadFalloutTextureRgba(path,art))return 0;
    return fo3hudassets::UploadTexture(art.width,art.height,art.rgba.data());
}
inline bool Ensure(){
    auto& s=State();
    if(s.attempted)return s.ready;
    s.attempted=true;
    if(!fo3hudrenderer::EnsureResources())return false;
    // The atlas aliases are named exactly by menus/main/hud_main_menu.xml.
    if(!fo3hudassets::ParseButtonTai(s.bracketSprite,
                                     "glow_XP_bar_over.dds")||
       !fo3hudassets::ParseButtonTai(s.pointerSprite,
                                     "glow_XP_bar_pointer.dds")) {
        __android_log_print(ANDROID_LOG_WARN,"FalloutQuest",
             "XP HUD: original InterfaceShared.tai sprites unavailable; text remains");
        return false;
    }
    s.bracket=OriginalAtlas(s.bracketSprite.atlasPath);
    s.pointer=s.bracketSprite.atlasPath==s.pointerSprite.atlasPath?
        s.bracket:OriginalAtlas(s.pointerSprite.atlasPath);
    if(!s.bracket||!s.pointer)return false;
    glGenVertexArrays(1,&s.vao);
    glGenBuffers(1,&s.vbo);
    s.ready=s.vao&&s.vbo;
    return s.ready;
}
inline void AppendSprite(std::vector<fo3hudassets::Vertex>& vertices,
                         const fo3hudassets::TaiSprite& a,
                         float x0,float y0,float x1,float y1,float z){
    fo3hudrenderer::AppendPromptQuad(vertices,x0,y0,x1,y1,z,
             a.u,a.v,a.u+a.w,a.v,a.u,a.v+a.h,a.u+a.w,a.v+a.h);
}
inline float Position(const fo3notify::ExperienceMeter& e,double age){
    const float first=fo3notify::Queue::Fraction(
        e.previous,e.thresholdStart,e.thresholdEnd);
    const float last=fo3notify::Queue::Fraction(
        e.total,e.thresholdStart,e.thresholdEnd);
    const float t=float(std::clamp(age/.7,0.,1.));
    // Smoothly advance, not a per-eye animation phase.
    return first+(last-first)*(t*t*(3.f-2.f*t));
}
inline void Prepare(){
    auto& r=State();
    auto& q=fo3notify::Notifications();
    const auto* xp=q.CurrentXP();
    r.visible=false;
    if(!xp)return;
    fo3hudrenderer::GlStateGuard guard;
    if(!Ensure())return;
    if(r.revision!=q.Revision()){
        r.revision=q.Revision();
        std::vector<fo3hudassets::Vertex> verts;
        // The source UI is 512x256 for the bracket with a 64x64 pointer.
        // Scale/anchor only are adapted for stereo readability in Quest.
        AppendSprite(verts,r.bracketSprite,-.32f,.28f,.23f,.035f,-2.f);
        r.bracketFirst=0;r.bracketCount=GLsizei(verts.size());
        r.pointerFirst=r.bracketCount;
        AppendSprite(verts,r.pointerSprite,-.29f,.19f,-.225f,.12f,-1.99f);
        r.pointerCount=GLsizei(verts.size())-r.pointerFirst;
        const auto& font=fo3hudrenderer::State();
        r.textFirst=GLsizei(verts.size());
        const float scale=.027f/std::max(1.f,font.baseLine);
        const auto label=[&](const std::string& word,float x,float y){
            fo3font::AppendText(font,word,0,0,
                [&](uint8_t,const fo3font::Glyph& g,const fo3font::Quad& a){
                    fo3hudrenderer::AppendPromptQuad(verts,
                       x+a.left*scale,y-a.top*scale,
                       x+a.right*scale,y-a.bottom*scale,-1.98f,
                       g.uv[0],g.uv[1],g.uv[2],g.uv[3],
                       g.uv[4],g.uv[5],g.uv[6],g.uv[7]);
                });
        };
        label(std::to_string(xp->level),-.30f,.32f);
        label(std::to_string(xp->level+1),.04f,.32f);
        label(std::to_string(xp->total)+" XP",.27f,.21f);
        r.textCount=GLsizei(verts.size())-r.textFirst;
        glBindVertexArray(r.vao);glBindBuffer(GL_ARRAY_BUFFER,r.vbo);
        glBufferData(GL_ARRAY_BUFFER,
            verts.size()*sizeof(fo3hudassets::Vertex),verts.data(),GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,
                             sizeof(fo3hudassets::Vertex),nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,
                             sizeof(fo3hudassets::Vertex),
                             reinterpret_cast<void*>(3*sizeof(float)));
    }
    // Only the six pointer vertices change, and only once before stereo draw.
    r.slide=Position(*xp,q.Age());
    std::array<fo3hudassets::Vertex,6> pointer{};
    std::vector<fo3hudassets::Vertex> v;
    const float x=-.29f+.25f*r.slide;
    AppendSprite(v,r.pointerSprite,x,.19f,x+.065f,.12f,-1.99f);
    std::copy_n(v.begin(),6,pointer.begin());
    glBindBuffer(GL_ARRAY_BUFFER,r.vbo);
    glBufferSubData(GL_ARRAY_BUFFER,r.pointerFirst*sizeof(fo3hudassets::Vertex),
                    sizeof(pointer),pointer.data());
    r.visible=true;
}
inline void Render(const float* mvp){
    const auto& s=State();
    const auto& hud=fo3hudrenderer::State();
    if(!s.visible||!s.ready||!mvp||!hud.ready)return;
    fo3hudrenderer::GlStateGuard guard;
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(hud.program);glUniformMatrix4fv(hud.mvpLoc,1,GL_FALSE,mvp);
    glUniform1i(hud.texLoc,0);glBindVertexArray(s.vao);
    glActiveTexture(GL_TEXTURE0);
    const float alpha=fo3notify::Notifications().Alpha();
    glUniform4f(hud.tintLoc,26.f/255,1.f,128.f/255,alpha);
    glBindTexture(GL_TEXTURE_2D,s.bracket);
    glDrawArrays(GL_TRIANGLES,s.bracketFirst,s.bracketCount);
    glBindTexture(GL_TEXTURE_2D,s.pointer);
    glDrawArrays(GL_TRIANGLES,s.pointerFirst,s.pointerCount);
    glBindTexture(GL_TEXTURE_2D,hud.fontTexture);
    glDrawArrays(GL_TRIANGLES,s.textFirst,s.textCount);
}
} // namespace fo3xpui
