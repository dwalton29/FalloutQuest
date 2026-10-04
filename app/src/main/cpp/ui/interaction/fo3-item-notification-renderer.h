#pragma once
#include "fo3-interaction-hud-renderer.h"
#include "fo3-item-notifications.h"
#include "fo3-hud-cached-state.h"

namespace fo3notifyui {
struct Resources {
    GLuint vao=0,vbo=0;
    GLsizei count=0;
    uint64_t revision=UINT64_MAX;
};
inline Resources& State() {static Resources r;return r;}
inline void Shutdown() {
    auto& r=State();
    if(r.vao)glDeleteVertexArrays(1,&r.vao);
    if(r.vbo)glDeleteBuffers(1,&r.vbo);
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
    if(!available) {queue.Clear();return;}
    queue.Advance(now);
    if(!fo3hudrenderer::State().ready) return;
    auto& r=State();auto& font=fo3hudrenderer::State();
    if(!r.vao) {glGenVertexArrays(1,&r.vao);glGenBuffers(1,&r.vbo);}
    if(r.revision==queue.Revision()) return;
    r.revision=queue.Revision();r.count=0;
    const std::string_view text(queue.Text());
    if(text.empty())return;
    std::vector<fo3hudassets::Vertex> vertices;vertices.reserve(text.size()*6);
    // HUDTemplates left-justified font 7. 460px Messages width, two rows within
    // its 90px height. Pixel->metre size and upper-right placement are VR-only.
    constexpr float scale=.00125f,wrap=460;
    size_t begin=0;int line=0;
    while(begin<text.size() && line<2) {
        size_t n=fo3font::FitText(font,text.substr(begin),wrap);
        if(!n)break;
        if(begin+n<text.size()) {
            const auto space=text.substr(begin,n).find_last_of(' ');
            if(space!=std::string_view::npos && space>0)n=space;
        }
        fo3font::AppendText(font,text.substr(begin,n),0,0,
            [&](uint8_t,const fo3font::Glyph& g,const fo3font::Quad& q) {
                fo3hudrenderer::AppendPromptQuad(vertices,.22f+q.left*scale,.43f-(q.top+line*45)*scale,
                    .22f+q.right*scale,.43f-(q.bottom+line*45)*scale,-2.0f,
                    g.uv[0],g.uv[1],g.uv[2],g.uv[3],g.uv[4],g.uv[5],g.uv[6],g.uv[7]);
            });
        begin+=n;while(begin<text.size() && text[begin]==' ')++begin;++line;
    }
    r.count=static_cast<GLsizei>(vertices.size());
    glBindVertexArray(r.vao);glBindBuffer(GL_ARRAY_BUFFER,r.vbo);
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(fo3hudassets::Vertex),vertices.data(),GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),nullptr);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(fo3hudassets::Vertex),reinterpret_cast<void*>(3*sizeof(float)));
}
inline void Render(const float* mvp) {
    const auto& r=State();const auto& font=fo3hudrenderer::State();
    const float alpha=fo3notify::Notifications().Alpha();
    if(!font.ready || !r.count || alpha<=0)return;
    const auto queries=fqgl::counters.queries;
    {
        fo3hudrenderer::CachedStateGuard guard;
        glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(font.program);glUniformMatrix4fv(font.mvpLoc,1,GL_FALSE,mvp);
        glUniform4f(font.tintLoc,26.0f/255,1,128.0f/255,alpha);glUniform1i(font.texLoc,0);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,font.fontTexture);
        glBindVertexArray(r.vao);glDrawArrays(GL_TRIANGLES,0,r.count);
    }
    if(fqgl::counters.queries!=queries)
        __android_log_print(ANDROID_LOG_ERROR,"FalloutQuest","ITEM ADDED HUD: unexpected driver query");
}
} // namespace fo3notifyui
