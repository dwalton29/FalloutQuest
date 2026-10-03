#pragma once
#include "fo3-loading-screen.h"
#include "fo3-loading-pose.h"
#include "rendering/mesh/fo3-static-nif.h"
#include "world/fo3-scene-preparation.h"
#include <cmath>
#include <cstddef>
#include <limits>

// Standalone loading renderer: CPU archive/NIF/DDS work runs off-thread.
// Draws into its own depth target, then replaces the entire eye image. No world,
// HUD or post-processing pixel can leak through the loading presentation.
namespace fo3loadingvr {
using fo3loadingpose::Matrix;
using fo3loadingpose::Multiply;
using fo3loadingpose::Placement;
struct Vertex { float p[3],n[3],uv[2],colour[4]; };
struct CpuShape { Fo3StaticNifMesh mesh; Fo3RgbaTexture image; std::vector<Vertex> vertices; };
struct CpuDisplay {
    Fo3RgbaTexture art;
    std::vector<CpuShape> model, compass;
    std::string modelPath, artPath;
};
struct Shape { GLuint vao=0,vbo=0,ibo=0,texture=0; GLsizei count=0; bool unlit=false; float alpha=1; };
inline fo3scene::Preparation<int> gCatalog;
inline fo3scene::Preparation<CpuDisplay> gPreparation;
inline bool gCatalogStarted=false,gCatalogFailed=false,gJobStarted=false,gUploaded=false;
inline uint64_t gGeneration=~uint64_t{0};
inline uint64_t gSeed=fo3loadingpose::Mix(Fo3LoadingClockUs());
inline Matrix gHead=fo3loadingpose::Identity(),gAnchor=fo3loadingpose::Identity();
inline std::vector<Shape> gModel,gCompass,gNextModel,gNextCompass;
inline size_t gModelUpload=0,gCompassUpload=0;
inline bool gArtUploaded=false;
inline GLuint gArt=0,gProgram=0,gQuad=0,gQuadBuffer=0;
inline GLuint gFramebuffer=0,gColour=0,gDepth=0;
inline int gWidth=0,gHeight=0;
inline float gArtAspect=4.0f/3.0f;
inline GLint gMvp=-1,gSampler=-1,gUnlit=-1,gTint=-1,gFade=-1;
inline uint64_t gVisibleStart=0;
inline float gFrameSeconds=0;

inline void PrepareCatalog() {
    if(gCatalogStarted || gCatalogFailed)return;
    gCatalogStarted=gCatalog.Start([](int&,const std::atomic<bool>& cancelled){
        if(cancelled.load())return false;
        fo3loading::Prepare(fo3loading::ESM_PATH,&cancelled);
        __android_log_print(ANDROID_LOG_INFO,"FalloutQuest","LOADING CATALOG: pictures=%zu exhibits=%zu",fo3loading::gScreens.size(),fo3loading::gModels.size());
        return !cancelled.load();
    });
    gCatalogFailed=!gCatalogStarted;
}
inline void SetHead(float x,float y,float z,float qx,float qy,float qz,float qw) {
    gHead=fo3loadingpose::Anchor(x,y,z,qx,qy,qz,qw);
}
inline void PrepareVertices(std::vector<CpuShape>& source,bool compass);
inline bool ReadShapes(const std::string& path,std::vector<CpuShape>& out,
                       const std::atomic<bool>& cancelled) {
    std::vector<Fo3StaticNifMesh> meshes;
    if(cancelled.load() || !LoadFo3StaticNifMeshes(path,meshes))return false;
    size_t vertices=0;
    for(const auto& m:meshes) {
        vertices+=m.positions.size()/3;
        if(m.skinned || vertices>200000)return false;
    }
    size_t imageBytes=0;
    for(auto& mesh:meshes) {
        if(cancelled.load())return false;
        if(mesh.positions.empty() || mesh.indices.empty())continue;
        CpuShape s;s.mesh=std::move(mesh);
        if(!s.mesh.diffuseTexturePath.empty() && imageBytes<32u*1024u*1024u)
            LoadFalloutTextureRgba(s.mesh.diffuseTexturePath,s.image);
        imageBytes+=s.image.rgba.size();
        out.push_back(std::move(s));
    }
    return !out.empty();
}
inline void DeleteShapes(std::vector<Shape>& shapes) {
    for(auto& s:shapes) {
        if(s.vao)glDeleteVertexArrays(1,&s.vao);
        if(s.vbo)glDeleteBuffers(1,&s.vbo);
        if(s.ibo)glDeleteBuffers(1,&s.ibo);
        if(s.texture)glDeleteTextures(1,&s.texture);
    }
    shapes.clear();
}
inline GLuint UploadImage(const Fo3RgbaTexture& image) {
    if(image.width<=0 || image.height<=0 || image.rgba.empty())return 0;
    GLuint texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
    return texture;
}
inline void PrepareVertices(std::vector<CpuShape>& source,bool compass) {
    float lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
    for(const auto& s:source) for(size_t i=0;i+2<s.mesh.positions.size();i+=3)
        for(int a=0;a<3;++a) {lo[a]=std::min(lo[a],s.mesh.positions[i+a]);hi[a]=std::max(hi[a],s.mesh.positions[i+a]);}
    float span[3]={hi[0]-lo[0],hi[1]-lo[1],hi[2]-lo[2]};
    const float extent=std::max({span[0],span[1],span[2]});
    if(!std::isfinite(extent) || extent<0.0001f)return;
    // NIF game coordinates are Z-up. UI NIFs can lie on another plane: use
    // their thinnest axis as depth while retaining the authored geometry.
    int ax=0,ay=2,az=1;
    if(compass) {
        az=static_cast<int>(std::min_element(span,span+3)-span);
        ax=az==0?1:0;ay=3-ax-az;
    }
    const float diagonal=std::sqrt(span[0]*span[0]+span[1]*span[1]+span[2]*span[2]);
    const float scale=compass?0.17f/extent:0.72f/diagonal;
    for(auto& src:source) {
        auto& m=src.mesh;
        std::vector<Vertex> v(m.positions.size()/3);
        for(size_t i=0;i<v.size();++i) {
            int axes[3]={ax,ay,az};
            for(int a=0;a<3;++a) {
                int axis=axes[a];float sign=(!compass&&a==2)?-1.0f:1.0f;
                v[i].p[a]=(m.positions[i*3+axis]-(lo[axis]+hi[axis])*0.5f)*scale*sign;
                v[i].n[a]=m.normals.size()==m.positions.size()?m.normals[i*3+axis]*sign:(a==2?1.0f:0.0f);
            }
            for(int a=0;a<2;++a)v[i].uv[a]=m.texcoords.size()==v.size()*2?m.texcoords[i*2+a]:0;
            for(int a=0;a<4;++a)v[i].colour[a]=m.vertexColors.size()==v.size()*4?m.vertexColors[i*4+a]:1;
        }
        src.vertices=std::move(v);
        // Interleave on the worker, retaining GPU material metadata and indices.
        m.positions.clear();m.normals.clear();m.texcoords.clear();m.vertexColors.clear();
    }
}
inline void UploadShape(CpuShape& src,std::vector<Shape>& target,bool compass) {
    auto& m=src.mesh;auto& v=src.vertices;
    if(v.empty() || m.indices.empty())return;
    Shape s;s.count=static_cast<GLsizei>(m.indices.size());s.unlit=compass||m.noLighting;s.alpha=m.alpha;
    s.texture=UploadImage(src.image);
    if(!s.texture) { Fo3RgbaTexture white;white.width=white.height=1;white.rgba={255,255,255,255};s.texture=UploadImage(white); }
    glGenVertexArrays(1,&s.vao);glBindVertexArray(s.vao);
    glGenBuffers(1,&s.vbo);glBindBuffer(GL_ARRAY_BUFFER,s.vbo);
    glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(Vertex),v.data(),GL_STATIC_DRAW);
    glGenBuffers(1,&s.ibo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,s.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,m.indices.size()*sizeof(uint32_t),m.indices.data(),GL_STATIC_DRAW);
    for(GLuint a=0;a<4;++a)glEnableVertexAttribArray(a);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,p)));
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,n)));
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,uv)));
    glVertexAttribPointer(3,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,colour)));
    target.push_back(s);

    src=CpuShape{};
}

inline bool EnsureProgram() {
    if(gProgram)return true;
    const char* vs=R"(#version 300 es
    precision highp float;
    layout(location=0) in vec3 aPosition;
    layout(location=1) in vec3 aNormal;
    layout(location=2) in vec2 aUv;
    layout(location=3) in vec4 aColour;
    uniform mat4 uMvp;
    out vec2 uv;out vec3 normal;out vec4 colour;
    void main(){gl_Position=uMvp*vec4(aPosition,1);uv=aUv;normal=aNormal;colour=aColour;})";
    const char* fs=R"(#version 300 es
    precision highp float;
    in vec2 uv;in vec3 normal;in vec4 colour;
    uniform sampler2D uImage;uniform float uUnlit;uniform vec4 uTint;uniform float uFade;
    out vec4 result;
    void main(){vec4 c=texture(uImage,uv)*colour*uTint;
    if(c.a<0.02)discard;
    float light=mix(0.35+0.65*max(dot(normalize(normal),normalize(vec3(-0.4,0.7,1))),0.0),1.0,uUnlit);
    result=vec4(c.rgb*light*uFade,c.a);})";
    GLuint v=fo3loading::Compile(GL_VERTEX_SHADER,vs),f=fo3loading::Compile(GL_FRAGMENT_SHADER,fs);
    if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return false;}
    gProgram=glCreateProgram();glAttachShader(gProgram,v);glAttachShader(gProgram,f);glLinkProgram(gProgram);
    glDeleteShader(v);glDeleteShader(f);GLint ok=0;glGetProgramiv(gProgram,GL_LINK_STATUS,&ok);
    if(!ok){glDeleteProgram(gProgram);gProgram=0;return false;}
    gMvp=glGetUniformLocation(gProgram,"uMvp");gSampler=glGetUniformLocation(gProgram,"uImage");
    gUnlit=glGetUniformLocation(gProgram,"uUnlit");gTint=glGetUniformLocation(gProgram,"uTint");gFade=glGetUniformLocation(gProgram,"uFade");
    const Vertex quad[]={{{-1,-1,0},{0,0,1},{0,0},{1,1,1,1}},{{1,-1,0},{0,0,1},{1,0},{1,1,1,1}},
        {{-1,1,0},{0,0,1},{0,1},{1,1,1,1}},{{1,1,0},{0,0,1},{1,1},{1,1,1,1}}};
    glGenVertexArrays(1,&gQuad);glBindVertexArray(gQuad);glGenBuffers(1,&gQuadBuffer);glBindBuffer(GL_ARRAY_BUFFER,gQuadBuffer);
    glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);
    for(GLuint a=0;a<4;++a)glEnableVertexAttribArray(a);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,p)));
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,n)));
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,uv)));
    glVertexAttribPointer(3,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,colour)));
    return true;
}
inline void AdvanceAssets() {
    const uint64_t generation=GetFo3LoadingGeneration();
    if(generation!=gGeneration) {
        gPreparation.Reset();DeleteShapes(gNextModel);DeleteShapes(gNextCompass);
        gModelUpload=gCompassUpload=0;gArtUploaded=false;
        gGeneration=generation;gAnchor=gHead;gJobStarted=gUploaded=false;gVisibleStart=Fo3LoadingClockUs();
    }
    PrepareCatalog();
    if(!gJobStarted && (gCatalog.Ready() || gCatalogFailed)) {
        const uint32_t cell=GetFo3LoadingCell(),world=GetFo3LoadingWorldspace();
        const uint64_t random=fo3loadingpose::Mix(gSeed^generation);
        const bool needCompass=gCompass.empty();
        gJobStarted=gPreparation.Start([cell,world,random,needCompass](CpuDisplay& out,const std::atomic<bool>& cancelled){
            const auto* art=fo3loading::Select(cell,world,random);
            if(art) {out.artPath=art->iconPath;LoadFalloutTextureRgba(art->iconPath,out.art);}
            if(needCompass)ReadShapes("Interface\\Circular Loading\\loading01.nif",out.compass,cancelled);
            if(!fo3loading::gModels.empty()) {
                const size_t start=random%fo3loading::gModels.size();
                for(size_t i=0;i<std::min<size_t>(8,fo3loading::gModels.size()) && !cancelled.load();++i) {
                    const auto& path=fo3loading::gModels[(start+i)%fo3loading::gModels.size()];
                    out.model.clear();
                    if(ReadShapes(path,out.model,cancelled)) {out.modelPath=path;break;}
                }
            }
            PrepareVertices(out.model,false);PrepareVertices(out.compass,true);
            return !cancelled.load();
        });
        if(!gJobStarted)gUploaded=true; // Thread creation failure must not trap a transition.
    }
    if(gJobStarted && gPreparation.Ready() && !gUploaded) {
        if(gPreparation.Successful()) {
            auto& cpu=gPreparation.Get();
            if(!gArtUploaded) {
                if(gArt)glDeleteTextures(1,&gArt);
                gArt=UploadImage(cpu.art);
                if(gArt)gArtAspect=static_cast<float>(cpu.art.width)/cpu.art.height;
                cpu.art=Fo3RgbaTexture{};gArtUploaded=true;
                return; // Separate artwork upload from mesh uploads.
            }
            if(gCompassUpload<cpu.compass.size()) {
                UploadShape(cpu.compass[gCompassUpload++],gNextCompass,true);return;
            }
            if(gModelUpload<cpu.model.size()) {
                UploadShape(cpu.model[gModelUpload++],gNextModel,false);return;
            }
            DeleteShapes(gModel);gModel.swap(gNextModel);
            if(!gNextCompass.empty()) {DeleteShapes(gCompass);gCompass.swap(gNextCompass);}
            __android_log_print(ANDROID_LOG_INFO,"FalloutQuest",
                "VR LOADING ASSETS: generation=%llu art=%s model=%s compassShapes=%zu panel=2.5m exhibit=2.1m",
                static_cast<unsigned long long>(generation),cpu.artPath.c_str(),cpu.modelPath.c_str(),gCompass.size());
            if(gCompass.empty())__android_log_print(ANDROID_LOG_WARN,"FalloutQuest","AUTHORED LOADING COMPASS UNAVAILABLE: Interface/Circular Loading/loading01.nif; check original mesh/texture archives");
        }
        gPreparation.Reset();gUploaded=true;gVisibleStart=Fo3LoadingClockUs();
    }
}
inline bool Ready() {return gUploaded && gGeneration==GetFo3LoadingGeneration();}
inline bool EnsureTarget(int width,int height) {
    if(!gFramebuffer)glGenFramebuffers(1,&gFramebuffer);
    if(!gColour)glGenTextures(1,&gColour);
    if(!gDepth)glGenRenderbuffers(1,&gDepth);
    glBindFramebuffer(GL_FRAMEBUFFER,gFramebuffer);
    if(width!=gWidth || height!=gHeight) {
        glBindTexture(GL_TEXTURE_2D,gColour);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glBindRenderbuffer(GL_RENDERBUFFER,gDepth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
        gWidth=width;gHeight=height;
    }
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,gColour,0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,gDepth);
    return glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
}
inline void Draw(const Shape& s,const Matrix& mvp,float green=0) {
    glUniformMatrix4fv(gMvp,1,GL_FALSE,mvp.m);glUniform1f(gUnlit,s.unlit?1:0);
    glUniform4f(gTint,green>0?26.0f/255:1,1,green>0?128.0f/255:1,s.alpha);
    glBindTexture(GL_TEXTURE_2D,s.texture);glBindVertexArray(s.vao);
    glDrawElements(GL_TRIANGLES,s.count,GL_UNSIGNED_INT,nullptr);
}
inline void Render(GLuint framebuffer,GLsizei width,GLsizei height,const float* viewProjection,bool advance) {
    if(!IsFo3LoadingVisible() || width<=0 || height<=0)return;
    GLint program,vao,buffer,active,texture,readFb,drawFb,rbo,viewport[4],depthFunc,blend[4];
    GLfloat clear[4];GLboolean mask,colourMask[4];
    glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFb);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFb);
    glGetIntegerv(GL_RENDERBUFFER_BINDING,&rbo);glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);
    glGetIntegerv(GL_BLEND_SRC_RGB,&blend[0]);glGetIntegerv(GL_BLEND_DST_RGB,&blend[1]);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&blend[2]);glGetIntegerv(GL_BLEND_DST_ALPHA,&blend[3]);
    glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);glGetBooleanv(GL_COLOR_WRITEMASK,colourMask);
    const GLenum caps[]={GL_DEPTH_TEST,GL_BLEND,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST};GLboolean enabled[5];
    for(int i=0;i<5;++i){enabled[i]=glIsEnabled(caps[i]);glDisable(caps[i]);}
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glClearColor(0,0,0,1);
    if(advance) {
        AdvanceAssets();
        gFrameSeconds=gVisibleStart?static_cast<float>(Fo3LoadingClockUs()-gVisibleStart)/1000000:0;
    }
    const bool target=EnsureTarget(width,height);
    if(target) {
        glViewport(0,0,width,height);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        if(EnsureProgram()) {
            Matrix vp;std::copy(viewProjection,viewProjection+16,vp.m);
            Matrix panel=Multiply(vp,gAnchor);
            glUseProgram(gProgram);glUniform1i(gSampler,0);
            const float seconds=gFrameSeconds;
            glUniform1f(gFade,std::clamp(seconds/0.35f,0.0f,1.0f));
            glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);
            if(gArt) {
                Matrix size=fo3loadingpose::Identity();size.m[0]=1.2f;size.m[5]=1.2f/gArtAspect;
                Matrix mvp=Multiply(panel,size);glUniformMatrix4fv(gMvp,1,GL_FALSE,mvp.m);
                glUniform1f(gUnlit,1);glUniform4f(gTint,1,1,1,1);glBindTexture(GL_TEXTURE_2D,gArt);glBindVertexArray(gQuad);
                glDrawArrays(GL_TRIANGLE_STRIP,0,4);
            }
            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            Matrix model=Multiply(panel,Placement(0,0,fo3loadingpose::PanelDistance-fo3loadingpose::ModelDistance,seconds*fo3loadingpose::RotationRadiansPerSecond));
            for(const auto& s:gModel)Draw(s,model);
            // Bottom right of the artwork. Rotate the real mesh as a rigid UI
            // emblem; this is not Gamebryo controller/Idle animation playback.
            Matrix spin=fo3loadingpose::Identity();float a=-seconds*0.8f;
            spin.m[0]=spin.m[5]=std::cos(a);spin.m[1]=std::sin(a);spin.m[4]=-std::sin(a);
            Matrix compass=Multiply(panel,Multiply(Placement(1.04f,-1.2f/gArtAspect+0.14f,0.10f,0),spin));
            glDisable(GL_DEPTH_TEST);
            for(const auto& s:gCompass)Draw(s,compass,1);
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER,gFramebuffer);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffer);
        glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    } else {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffer);glClear(GL_COLOR_BUFFER_BIT);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER,readFb);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFb);glBindRenderbuffer(GL_RENDERBUFFER,rbo);
    glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glClearColor(clear[0],clear[1],clear[2],clear[3]);
    glColorMask(colourMask[0],colourMask[1],colourMask[2],colourMask[3]);glDepthMask(mask);glDepthFunc(depthFunc);
    glBlendFuncSeparate(blend[0],blend[1],blend[2],blend[3]);
    for(int i=0;i<5;++i)if(enabled[i])glEnable(caps[i]);else glDisable(caps[i]);
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);glUseProgram(program);
    glBindTexture(GL_TEXTURE_2D,texture);glActiveTexture(active);
}
inline void Shutdown() {
    gPreparation.Reset();gCatalog.Reset();
    fo3loading::gScreens.clear();fo3loading::gModels.clear();fo3loading::gScreensPrepared=false;
    gCatalogStarted=gCatalogFailed=false;gJobStarted=gUploaded=false;
    DeleteShapes(gModel);DeleteShapes(gCompass);DeleteShapes(gNextModel);DeleteShapes(gNextCompass);
    if(gArt)glDeleteTextures(1,&gArt);
    if(gColour)glDeleteTextures(1,&gColour);
    if(gDepth)glDeleteRenderbuffers(1,&gDepth);
    if(gFramebuffer)glDeleteFramebuffers(1,&gFramebuffer);
    if(gQuad)glDeleteVertexArrays(1,&gQuad);
    if(gQuadBuffer)glDeleteBuffers(1,&gQuadBuffer);
    if(gProgram)glDeleteProgram(gProgram);
    gArt=gColour=gDepth=gFramebuffer=gQuad=gQuadBuffer=gProgram=0;gWidth=gHeight=0;
    gGeneration=~uint64_t{0};
}
} // namespace fo3loadingvr
inline void RenderFo3LoadingVr(GLuint framebuffer,GLsizei width,GLsizei height,const float* viewProjection,bool advance) {
    fo3loadingvr::Render(framebuffer,width,height,viewProjection,advance);
}
inline void ShutdownFo3LoadingVr() {fo3loadingvr::Shutdown();ShutdownFo3LoadingScreenBase();}
