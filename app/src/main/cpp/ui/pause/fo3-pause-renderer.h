#pragma once
#include "ui/pause/fo3-debug-menu.h"
#include "ui/interaction/fo3-interaction-hud-renderer.h"

namespace fo3pauseui {
struct Resources {
    GLuint vao=0,vbo=0,solid=0,art=0,darken=0,fontTexture=0;
    fo3hudassets::Font font;
    uint64_t revision=UINT64_MAX;
    GLsizei count=0;
    bool attempted=false;
    struct Draw {GLint first;GLsizei count;GLuint texture;float alpha;bool green;float brightness;};
    std::vector<Draw> draws;
};
inline Resources& State(){static Resources r;return r;}
inline void Shutdown(){auto& r=State();if(r.vbo)glDeleteBuffers(1,&r.vbo);if(r.vao)glDeleteVertexArrays(1,&r.vao);
    for(GLuint t:{r.solid,r.art,r.darken,r.fontTexture}) {if(t)glDeleteTextures(1,&t);}
    r={};}
inline void Prepare(const fo3debug::Menu& menu){
    if(!menu.panel.visible)return;
    fo3hudrenderer::GlStateGuard guard;
    if(!fo3hudrenderer::EnsureResources())return;
    auto& r=State();
    if(!r.attempted){
        r.attempted=true;
        glGenVertexArrays(1,&r.vao);glGenBuffers(1,&r.vbo);
        const uint8_t white[]={255,255,255,255};r.solid=fo3hudassets::UploadTexture(1,1,white);
        auto texture=[](const char* path){Fo3RgbaTexture t;return LoadFalloutTextureRgba(path,t)?fo3hudassets::UploadTexture(t.width,t.height,t.rgba.data()):GLuint(0);};
        r.art=texture("Textures\\Interface\\Pause\\pausescreen01.dds");
        r.darken=texture("Textures\\Interface\\Pause\\pausescreendarken01.dds");
        // button_basic.xml _font=2; FALLOUT.INI maps it to Monofonto_Large.
        if(fo3hudassets::ParseFont(r.font,"Textures\\Fonts\\Monofonto_Large.fnt"))
            r.fontTexture=fo3hudassets::UploadTexture(r.font.textureWidth,r.font.textureHeight,r.font.rgba.data());
    }
    if(r.revision==menu.panel.revision||!r.vao||!r.vbo)return;
    r.revision=menu.panel.revision;r.draws.clear();
    std::vector<fo3hudassets::Vertex> v;
    auto quad=[&](float x,float y,float w,float h){fo3hudrenderer::AppendPromptQuad(v,x,y,x+w,y-h,0,0,0,1,0,0,1,1,1);};
    auto record=[&](size_t start,GLuint tex,float alpha,bool green,float brightness=1.f){r.draws.push_back({GLint(start),GLsizei(v.size()-start),tex,alpha,green,brightness});};
    quad(-.76f,.48f,1.52f,.96f);record(0,r.solid,1,false);
    if(r.art){const auto start=v.size();quad(-.76f,.48f,1.52f,.96f);record(start,r.art,1,false);}
    if(r.darken){const auto start=v.size();quad(-.76f,.48f,1.52f,.96f);record(start,r.darken,1,false);}
    const bool root=menu.page==fo3debug::Menu::Page::Root;
    if(!root){const auto start=v.size();quad(-.76f,.48f,1.52f,.96f);record(start,r.solid,.93f,false);}
    const auto& fallback=fo3hudrenderer::State();
    const fo3font::Metrics& font=r.fontTexture?static_cast<const fo3font::Metrics&>(r.font):static_cast<const fo3font::Metrics&>(fallback);
    const GLuint tex=r.fontTexture?r.fontTexture:fallback.fontTexture;
    const float scale=(root?.037f:.026f)/std::max(1.f,font.baseLine);
    auto text=[&](const std::string& s,float x,float y,float width,int maxLines,float brightness=1.f){
        const auto first=v.size();int row=0;
        for(const auto& line:fo3dialogue::Wrap(font,s,width/scale)){
            if(row++>=maxLines)break;
            fo3font::AppendText(font,line,0,0,[&](uint8_t,const fo3font::Glyph& g,const fo3font::Quad& q){
                fo3hudrenderer::AppendPromptQuad(v,x+q.left*scale,y-q.top*scale,x+q.right*scale,y-q.bottom*scale,0,
                    g.uv[0],g.uv[1],g.uv[2],g.uv[3],g.uv[4],g.uv[5],g.uv[6],g.uv[7]);});
            y-=.031f;
        }record(first,tex,1,true,brightness);
    };
    if(!root){text(menu.panel.title,-.69f,.43f,1.38f,1);text(menu.panel.subtitle,-.69f,.385f,1.38f,4);}
    const float step=root?.072f:.041f; // pause_menu.xml: 60 pixels at .0012m/px.
    const float top=root?.20f:.21f;
    for(size_t i=0;i<menu.panel.choices.size();++i){
        const auto& label=menu.panel.choices[i];float x=-.68f;
        if(root)x=.67f-fo3font::MeasureText(font,label)*scale; // authored right edge.
        const float y=top-float(i)*step;
        if(i==menu.selected){const auto first=v.size();
            const float w=root?fo3font::MeasureText(font,label)*scale+.048f:1.38f;
            quad(x-.024f,y+.009f,w,.044f);record(first,r.solid,32.f/255,true,128.f/255);
            const auto border=v.size();quad(x-.024f,y+.009f,w,.0015f);quad(x-.024f,y-.0335f,w,.0015f);
            quad(x-.024f,y+.009f,.0015f,.044f);quad(x+w-.0255f,y+.009f,.0015f,.044f);record(border,r.solid,1,true,200.f/255);
        }
        text(label,x,y,root?.80f:1.36f,1,root&&i>0&&i<5?128.f/255:1.f);
    }
    text(menu.panel.footer,-.69f,-.385f,1.38f,2);
    glBindVertexArray(r.vao);glBindBuffer(GL_ARRAY_BUFFER,r.vbo);
    glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(fo3hudassets::Vertex),v.data(),GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),nullptr);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),reinterpret_cast<const void*>(3*sizeof(float)));
    r.count=GLsizei(v.size());
}
inline void Render(const float* mvp){
    const auto& r=State();const auto& font=fo3hudrenderer::State();if(!font.ready||!r.count)return;
    fo3hudrenderer::GlStateGuard guard;glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(font.program);glUniformMatrix4fv(font.mvpLoc,1,GL_FALSE,mvp);glUniform1i(font.texLoc,0);glActiveTexture(GL_TEXTURE0);glBindVertexArray(r.vao);
    for(const auto& draw:r.draws){glBindTexture(GL_TEXTURE_2D,draw.texture);
        if(draw.green)glUniform4f(font.tintLoc,26.f/255*draw.brightness,draw.brightness,128.f/255*draw.brightness,draw.alpha);
        else if(draw.texture==r.solid)glUniform4f(font.tintLoc,0,0,0,draw.alpha);
        else glUniform4f(font.tintLoc,1,1,1,draw.alpha);
        glDrawArrays(GL_TRIANGLES,draw.first,draw.count);
    }
}
}
