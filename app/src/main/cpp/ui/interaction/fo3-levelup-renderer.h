#pragma once
// LevelUpMenu from the user's Fallout - Misc.bsa: two-page 1024x800
// bracket title, 470px left list, right illustration and selection info.
// VR changes only scale/position and input; font/perk ICON come from installed
// original assets. No replacement artwork is embedded.
#include "ui/interaction/fo3-dialogue-panel-renderer.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace fo3levelui {
struct Resources {
    GLuint vao=0,vbo=0,solid=0,background=0,illustration=0;
    std::string loadedIcon;
    std::unordered_map<std::string,GLuint> illustrationCache;
    uint64_t revision=UINT64_MAX;
    GLsizei bgCount=0,linesCount=0,highlightCount=0,textCount=0,iconCount=0;
    GLint total=0;
    bool iconAttempted=false;
};
inline Resources& State(){static Resources r;return r;}
inline void Shutdown(){
    auto& r=State();
    if(r.vbo)glDeleteBuffers(1,&r.vbo);
    if(r.vao)glDeleteVertexArrays(1,&r.vao);
    if(r.solid)glDeleteTextures(1,&r.solid);
    if(r.background)glDeleteTextures(1,&r.background);
    for(const auto& art:r.illustrationCache)
        if(art.second)glDeleteTextures(1,&art.second);
    r={};
}
inline void Quad(std::vector<fo3hudassets::Vertex>& v,float x0,float y0,
                 float x1,float y1,float z=0,
                 float u0=0,float v0=0,float u1=1,float v1=1){
    fo3hudrenderer::AppendPromptQuad(v,x0,y0,x1,y1,z,
                                     u0,v0,u1,v0,u0,v1,u1,v1);
}
inline void Prepare(const fo3dialogue::Panel& p,const std::string& iconPath){
    if(!p.visible)return;
    fo3hudrenderer::GlStateGuard guard;
    if(!fo3hudrenderer::EnsureResources())return;
    auto& font=fo3hudrenderer::State();
    auto& r=State();
    if(!r.vao){
        glGenVertexArrays(1,&r.vao);
        glGenBuffers(1,&r.vbo);
        const uint8_t white[]{255,255,255,255};
        r.solid=fo3hudassets::UploadTexture(1,1,white);
        Fo3RgbaTexture black;
        if(LoadFalloutTextureRgba(
            "Textures\\Interface\\Shared\\Background\\solid_black.dds",black))
            r.background=fo3hudassets::UploadTexture(
                black.width,black.height,black.rgba.data());
    }
    if(!r.vao||!r.vbo||!r.solid)return;
    if(r.loadedIcon!=iconPath){
        r.loadedIcon=iconPath;
        r.illustration=0;
        r.iconAttempted=false;
    }
    if(!iconPath.empty()&&!r.iconAttempted){
        r.iconAttempted=true;
        const auto cached=r.illustrationCache.find(iconPath);
        if(cached!=r.illustrationCache.end()){
            r.illustration=cached->second;
        }else{
            Fo3RgbaTexture art;
            // Read original AVIF/PERK ICON only on the logical frame.
            if(LoadFalloutTextureRgba(iconPath,art))
                r.illustration=fo3hudassets::UploadTexture(
                    art.width,art.height,art.rgba.data());
            // The eligible perk/skill set is small and bounded.
            if(r.illustrationCache.size()<32)
                r.illustrationCache.emplace(iconPath,r.illustration);
            if(!r.illustration)__android_log_print(ANDROID_LOG_WARN,
                "FalloutQuest","LEVELUP original ICON unavailable: %s",
                iconPath.c_str());
        }
    }
    if(r.revision==p.revision)return;
    r.revision=p.revision;
    std::vector<fo3hudassets::Vertex> vert;
    vert.reserve(14000);
    // 1024x800 authored plane mapped to 1.16x.90m, 2D pixel origin at top
    // left. The split and bracket positions follow LevelUpMenu's rectangles.
    constexpr float scale=.00112f, left=-.575f, top=.45f;
    const auto X=[=](float a){return left+a*scale;};
    const auto Y=[=](float a){return top-a*scale;};
    Quad(vert,X(-32),Y(-78),X(1054),Y(875),0); // solid_black backdrop
    r.bgCount=GLsizei(vert.size());
    auto line=[&](float x,float y,float w,float h){
        Quad(vert,X(x),Y(y),X(x+w),Y(y+h),.002f);
    };
    // Recovered headline bracket and vertical dividers; white base tint is
    // replaced with original HUDMain green in Render().
    line(0,20,54,2);line(58,20,2,19);
    line(420,20,595,2);line(1015,20,2,20);
    line(475,80,2,612);
    line(490,382,510,2);line(490,694,510,2);
    line(0,705,1018,2);
    r.linesCount=GLsizei(vert.size())-r.bgCount;
    const size_t selected=p.selected;
    const size_t start=selected>=14?selected-13:0;
    const size_t visible=std::min(size_t(15),p.choices.size()-std::min(start,p.choices.size()));
    const bool selectedVisible=selected>=start&&selected<start+visible;
    if(selectedVisible) {
        const float y=93+float(selected-start)*39;
        line(12,y,448,36);
    }
    r.highlightCount=GLsizei(vert.size())-r.bgCount-r.linesCount;
    const float fontScale=.027f/std::max(1.f,font.baseLine);
    auto text=[&](const std::string& str,float x,float y,float wrap,
                  float lineStep,int maxLines){
        if(str.empty())return;
        const auto lines=fo3dialogue::Wrap(font,str,wrap/fontScale);
        int row=0;
        for(const auto& s:lines){
            if(row>=maxLines)break;
            fo3font::AppendText(font,s,0,0,[&](uint8_t,
                       const fo3font::Glyph& g,const fo3font::Quad& q){
                Quad(vert,X(x)+q.left*fontScale,Y(y)-q.top*fontScale,
                     X(x)+q.right*fontScale,Y(y)-q.bottom*fontScale,.009f,
                     g.uv[0],g.uv[1],g.uv[6],g.uv[7]);
            });
            ++row;y+=lineStep;
        }
    };
    text(p.title,75,7,860*scale,33,1);
    const size_t count=p.choices.size();
    for(size_t i=start;i<count&&i<start+15;++i)
        text(p.choices[i],18,83+39*float(i-start),445*scale,39,1);
    // The original right half has a large perk icon above LUM_SelectionText.
    // Selection text must stay below the art, never overlap it.
    text(p.subtitle,502,403,495*scale,36,8);
    text(p.footer,24,733,978*scale,42,2);
    r.textCount=GLsizei(vert.size())-r.bgCount-r.linesCount-r.highlightCount;
    if(r.illustration){
        Quad(vert,X(577),Y(56),X(934),Y(372),.011f);
    }
    r.iconCount=GLsizei(vert.size())-
        r.bgCount-r.linesCount-r.highlightCount-r.textCount;
    r.total=GLint(vert.size());
    glBindVertexArray(r.vao);
    glBindBuffer(GL_ARRAY_BUFFER,r.vbo);
    glBufferData(GL_ARRAY_BUFFER,vert.size()*sizeof(fo3hudassets::Vertex),
                 vert.data(),GL_DYNAMIC_DRAW);
    fo3hudassets::UploadVertices(r.vao,vert.data(),vert.size());
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,
                         sizeof(fo3hudassets::Vertex),nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,
                         sizeof(fo3hudassets::Vertex),
                         reinterpret_cast<const void*>(3*sizeof(float)));
}
inline void Render(const float* mvp,const fo3dialogue::Panel& panel){
    if(!panel.visible||!mvp)return;
    const auto& r=State();
    const auto& font=fo3hudrenderer::State();
    if(!font.ready||!r.vao||!r.solid||!r.total)return;
    fo3hudrenderer::GlStateGuard guard;
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(font.program);
    glUniformMatrix4fv(font.mvpLoc,1,GL_FALSE,mvp);
    glUniform1i(font.texLoc,0);
    glActiveTexture(GL_TEXTURE0);glBindVertexArray(r.vao);
    glBindTexture(GL_TEXTURE_2D,r.background?r.background:r.solid);
    glUniform4f(font.tintLoc,r.background?1.f:.008f,
        r.background?1.f:.017f,r.background?1.f:.011f,.94f);
    glDrawArrays(GL_TRIANGLES,0,r.bgCount);
    glBindTexture(GL_TEXTURE_2D,r.solid);
    glUniform4f(font.tintLoc,26.f/255,1.f,128.f/255,.8f);
    glDrawArrays(GL_TRIANGLES,r.bgCount,r.linesCount);
    if(r.highlightCount){
        glUniform4f(font.tintLoc,26.f/255,1.f,128.f/255,.20f);
        glDrawArrays(GL_TRIANGLES,r.bgCount+r.linesCount,r.highlightCount);
    }
    glBindTexture(GL_TEXTURE_2D,font.fontTexture);
    glUniform4f(font.tintLoc,26.f/255,1.f,128.f/255,1);
    const GLsizei textStart=r.bgCount+r.linesCount+r.highlightCount;
    glDrawArrays(GL_TRIANGLES,textStart,r.textCount);
    if(r.illustration&&r.iconCount){
        glBindTexture(GL_TEXTURE_2D,r.illustration);
        glUniform4f(font.tintLoc,26.f/255,1.f,128.f/255,1);
        glDrawArrays(GL_TRIANGLES,textStart+r.textCount,r.iconCount);
    }
}
} // namespace fo3levelui
