#pragma once
#include "../../data/fo3-install-paths.h"
#include "fo3-loading-screen.h"
#include "fo3-loading-pose.h"
#include "fo3-loading-menu.h"
#include "fo3-loading-slideshow.h"
#include "audio/fo3-audio.h"
#include "data/fo3-asset-store.h"
#include "data/fo3-bsa-reader.h"
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
struct CpuShape { Fo3StaticNifMesh mesh; Fo3RgbaTexture image; std::vector<Vertex> vertices; int bone=-1,slide=-1; };
struct CpuDisplay {
    Fo3RgbaTexture art;
    std::vector<CpuShape> model, compass, overlay;
    std::string modelPath, artPath;
    std::vector<Fo3RgbaTexture> slides;
    fo3anim::Skeleton hierarchy;
    std::vector<fo3anim::Clip> clips;
    std::vector<int> blockBones;
    fo3anim::Matrix uiToPanel=fo3anim::Identity(),panelToUi=fo3anim::Identity();
    float halfHeight=0.68f;
    fo3anim::Skeleton compassHierarchy;
    std::vector<fo3anim::Clip> compassClips;
    std::vector<int> compassBlocks;
    fo3anim::Matrix compassToPanel=fo3anim::Identity(),panelToCompass=fo3anim::Identity();
};
struct Shape { GLuint vao=0,vbo=0,ibo=0,texture=0; GLsizei count=0; bool unlit=false,blend=false; uint8_t sourceBlend=6,destBlend=7; float alpha=1,cutoff=0.02f; int bone=-1,slide=-1; float centre[3]{}; };
inline fo3scene::Preparation<fo3loadingmenu::Definition> gCatalog;
inline fo3loadingmenu::Definition gMenu;
inline fo3scene::Preparation<CpuDisplay> gPreparation;
inline bool gCatalogStarted=false,gCatalogFailed=false,gJobStarted=false,gUploaded=false;
inline uint64_t gGeneration=~uint64_t{0};
inline uint64_t gSeed=fo3loadingpose::Mix(Fo3LoadingClockUs());
inline Matrix gHead=fo3loadingpose::Identity(),gAnchor=fo3loadingpose::Identity();
inline std::vector<Shape> gModel,gCompass,gOverlay,gNextModel,gNextCompass,gNextOverlay;
inline size_t gModelUpload=0,gCompassUpload=0,gOverlayUpload=0;
inline bool gArtUploaded=false;
inline std::vector<GLuint> gSlides,gNextSlides;
inline size_t gSlideUpload=0;
inline fo3anim::Skeleton gUiHierarchy;
inline std::vector<fo3anim::Clip> gUiClips;
inline fo3anim::Matrix gUiToPanel=fo3anim::Identity(),gPanelToUi=fo3anim::Identity();
inline float gUiHalfHeight=0.68f;
inline fo3slideshow::Player gSlideshow;
inline uint64_t gSlideStarted=0;
inline std::vector<Matrix> gUiDeltas;
inline fo3anim::Skeleton gCompassHierarchy;
inline std::vector<fo3anim::Clip> gCompassClips;
inline fo3anim::Pose gCompassPose;
inline int gCompassClip=-1;
inline fo3anim::Matrix gCompassToPanel=fo3anim::Identity(),gPanelToCompass=fo3anim::Identity();
inline std::vector<Matrix> gCompassDeltas;
inline std::vector<size_t> gUiDrawOrder;
inline GLuint gArt=0,gProgram=0,gQuad=0,gQuadBuffer=0;
inline GLuint gFramebuffer=0,gColour=0,gDepth=0;
inline int gWidth=0,gHeight=0;
inline float gArtAspect=4.0f/3.0f;
inline GLint gMvp=-1,gSampler=-1,gUnlit=-1,gTint=-1,gFade=-1,gCutoff=-1,gClipUi=-1,gUiTransform=-1,gClipHalf=-1;
inline uint64_t gVisibleStart=0,gRotationStart=0;
inline float gFrameRotationSeconds=0;
inline float gFrameSeconds=0;

inline void PrepareCatalog() {
    if(gCatalogStarted || gCatalogFailed)return;
    gCatalogStarted=gCatalog.Start([](fo3loadingmenu::Definition& menu,const std::atomic<bool>& cancelled){
        if(cancelled.load())return false;
        fo3loading::Prepare(fo3assets::FalloutMasterPath().c_str(),&cancelled);
        std::vector<uint8_t> xml;
        if(fo3assets::GetBsaArchive(fo3assets::FalloutDataPath("Fallout - Misc.bsa"))->Read("menus/loading_menu.xml",xml,nullptr,fo3assets::BsaPathKind::Exact,1024u*1024u))
            fo3loadingmenu::ReadXml(std::string(xml.begin(),xml.end()),menu);
        for(const char* filename:{"FALLOUT.INI","Fallout.ini"}) {
            FILE* ini=std::fopen(fo3assets::FalloutDataPath(filename).c_str(),"rb");
            if(!ini)continue;
            std::string text;char buffer[4096];size_t size=0;
            while((size=std::fread(buffer,1,sizeof(buffer),ini))>0 && text.size()<1024u*1024u)text.append(buffer,size);
            std::fclose(ini);fo3loadingmenu::ReadIni(text,menu);break;
        }
        __android_log_print(ANDROID_LOG_INFO,"FalloutQuest","LOADING CATALOG: pictures=%zu exhibits=%zu",fo3loading::gScreens.size(),fo3loading::gModels.size());
        return !cancelled.load();
    });
    gCatalogFailed=!gCatalogStarted;
}
inline void SetHead(float x,float y,float z,float qx,float qy,float qz,float qw) {
    gHead=fo3loadingpose::Anchor(x,y,z,qx,qy,qz,qw);
}
inline void PrepareVertices(std::vector<CpuShape>& source,bool compass,float uiExtent=0.17f,fo3anim::Matrix* mapping=nullptr,const fo3anim::Pose* initialPose=nullptr);
inline bool ReadShapes(const std::string& path,std::vector<CpuShape>& out,
                       const std::atomic<bool>& cancelled,bool originalUi=false) {
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
        if(originalUi && (s.mesh.diffuseTexturePath.empty() || s.image.rgba.empty())) {
            __android_log_print(ANDROID_LOG_WARN,"FalloutQuest","LOADING UI MATERIAL MISS: model=%s texture=%s; skipping unresolved shape",path.c_str(),s.mesh.diffuseTexturePath.c_str());
            continue;
        }
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
inline void PrepareVertices(std::vector<CpuShape>& source,bool compass,float uiExtent,fo3anim::Matrix* mapping,const fo3anim::Pose* initialPose) {
    float lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
    for(const auto& s:source) for(size_t i=0;i+2<s.mesh.positions.size();i+=3) {
        std::array<float,3> point{s.mesh.positions[i],s.mesh.positions[i+1],s.mesh.positions[i+2]};
        // Embedded Idle rotates the authored XZ rest geometry into the XY UI
        // plane. Choose bounds and axes from the displayed pose, not rest.
        if(initialPose && s.bone>=0 && static_cast<size_t>(s.bone)<initialPose->delta.size())
            point=fo3anim::Point(initialPose->delta[s.bone],point);
        for(int a=0;a<3;++a) {lo[a]=std::min(lo[a],point[a]);hi[a]=std::max(hi[a],point[a]);}
    }
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
    const float scale=compass?uiExtent/extent:0.72f/diagonal;
    if(mapping) {
        *mapping=fo3anim::Identity();
        for(int c=0;c<3;++c)for(int r=0;r<3;++r)(*mapping)[c*4+r]=0;
        const int axes[3]={ax,ay,az};
        for(int a=0;a<3;++a) {
            const float sign=(!compass&&a==2)?-1.0f:1.0f;
            (*mapping)[axes[a]*4+a]=scale*sign;
            (*mapping)[12+a]=-(lo[axes[a]]+hi[axes[a]])*0.5f*scale*sign;
        }
    }
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
// Map the original background plane, rather than the bounds of off-screen
// slides, to the user's 2.4m panel. Preserve all authored layer depth offsets.
inline bool PrepareOverlay(CpuDisplay& display) {
    float lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
    bool background=false;
    for(const auto& shape:display.overlay) {
        if(fo3loadingmenu::Lower(shape.mesh.diffuseTexturePath).find("loading_background.dds")==std::string::npos)continue;
        for(size_t i=0;i<shape.mesh.positions.size();i+=3)
            for(int a=0;a<3;++a) {lo[a]=std::min(lo[a],shape.mesh.positions[i+a]);hi[a]=std::max(hi[a],shape.mesh.positions[i+a]);}
        background=true;
    }
    if(!background || hi[0]-lo[0]<1 || hi[1]-lo[1]<1)return false;
    const float scale=2.4f/(hi[0]-lo[0]);
    auto& map=display.uiToPanel;
    map=fo3anim::Identity();map[0]=map[5]=scale;map[10]=-scale;
    map[12]=-(lo[0]+hi[0])*0.5f*scale;map[13]=-(lo[1]+hi[1])*0.5f*scale;map[14]=hi[2]*scale;
    if(!fo3anim::Inverse(map,display.panelToUi))return false;
    display.halfHeight=(hi[1]-lo[1])*scale*0.5f;
    for(auto& shape:display.overlay) {
        auto& mesh=shape.mesh;
        if(mesh.shapeBlock<display.blockBones.size())shape.bone=display.blockBones[mesh.shapeBlock];
        const auto path=fo3loadingmenu::Lower(mesh.diffuseTexturePath);
        if(path.find("loading_screen01.dds")!=std::string::npos)shape.slide=0;
        if(path.find("loading_screen02.dds")!=std::string::npos)shape.slide=1;
        shape.vertices.resize(mesh.positions.size()/3);
        for(size_t i=0;i<shape.vertices.size();++i) {
            auto& v=shape.vertices[i];
            auto point=fo3anim::Point(map,{mesh.positions[i*3],mesh.positions[i*3+1],mesh.positions[i*3+2]});
            std::copy(point.begin(),point.end(),v.p);v.n[0]=v.n[1]=0;v.n[2]=1;
            for(int a=0;a<2;++a)v.uv[a]=mesh.texcoords.size()==shape.vertices.size()*2?mesh.texcoords[i*2+a]:0;
            if(shape.slide>=0)v.uv[1]=1.0f-v.uv[1];
            for(int a=0;a<4;++a)v.colour[a]=mesh.vertexColors.size()==shape.vertices.size()*4?mesh.vertexColors[i*4+a]:1;
        }
        mesh.positions.clear();mesh.normals.clear();mesh.texcoords.clear();mesh.vertexColors.clear();
    }
    return true;
}
inline void DeleteImages(std::vector<GLuint>& images) {
    for(GLuint texture:images)if(texture)glDeleteTextures(1,&texture);
    images.clear();
}
inline void UploadShape(CpuShape& src,std::vector<Shape>& target,bool compass) {
    auto& m=src.mesh;auto& v=src.vertices;
    if(v.empty() || m.indices.empty())return;
    Shape s;s.bone=src.bone;s.slide=src.slide;s.count=static_cast<GLsizei>(m.indices.size());s.unlit=compass||m.noLighting;s.alpha=m.alpha;s.blend=m.alphaBlend;s.sourceBlend=m.alphaSourceBlend;s.destBlend=m.alphaDestBlend;
    for(const auto& vertex:v)for(int a=0;a<3;++a)s.centre[a]+=vertex.p[a]/v.size();
    s.cutoff=m.alphaTest?m.alphaThreshold:0.0f;
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
    uniform mat4 uMvp;uniform mat4 uUiTransform;
    out vec2 uv;out vec3 normal;out vec4 colour;out vec2 uiPosition;
    void main(){gl_Position=uMvp*vec4(aPosition,1);uv=aUv;normal=aNormal;colour=aColour;uiPosition=(uUiTransform*vec4(aPosition,1)).xy;})";
    const char* fs=R"(#version 300 es
    precision highp float;
    in vec2 uv;in vec3 normal;in vec4 colour;in vec2 uiPosition;
    uniform float uClipUi;uniform vec2 uClipHalf;
    uniform sampler2D uImage;uniform float uUnlit;uniform vec4 uTint;uniform float uFade;uniform float uCutoff;
    out vec4 result;
    void main(){if(uClipUi>0.5 && any(greaterThan(abs(uiPosition),uClipHalf)))discard;vec4 c=texture(uImage,uv)*colour*uTint;
    if(c.a<uCutoff)discard;
    float light=uUnlit>0.5?1.0:0.35+0.65*max(dot(normalize(normal),normalize(vec3(-0.4,0.7,1))),0.0);
    result=vec4(c.rgb*light*uFade,c.a);})";
    GLuint v=fo3loading::Compile(GL_VERTEX_SHADER,vs),f=fo3loading::Compile(GL_FRAGMENT_SHADER,fs);
    if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return false;}
    gProgram=glCreateProgram();glAttachShader(gProgram,v);glAttachShader(gProgram,f);glLinkProgram(gProgram);
    glDeleteShader(v);glDeleteShader(f);GLint ok=0;glGetProgramiv(gProgram,GL_LINK_STATUS,&ok);
    if(!ok){glDeleteProgram(gProgram);gProgram=0;return false;}
    gMvp=glGetUniformLocation(gProgram,"uMvp");gSampler=glGetUniformLocation(gProgram,"uImage");
    gUnlit=glGetUniformLocation(gProgram,"uUnlit");gTint=glGetUniformLocation(gProgram,"uTint");gFade=glGetUniformLocation(gProgram,"uFade");gCutoff=glGetUniformLocation(gProgram,"uCutoff");
    gClipUi=glGetUniformLocation(gProgram,"uClipUi");gUiTransform=glGetUniformLocation(gProgram,"uUiTransform");gClipHalf=glGetUniformLocation(gProgram,"uClipHalf");
    const Vertex quad[]={{{-1,-1,0},{0,0,1},{0,1},{1,1,1,1}},{{1,-1,0},{0,0,1},{1,1},{1,1,1,1}},
        {{-1,1,0},{0,0,1},{0,0},{1,1,1,1}},{{1,1,0},{0,0,1},{1,0},{1,1,1,1}}};
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
        gPreparation.Reset();DeleteShapes(gNextModel);DeleteShapes(gNextCompass);DeleteShapes(gNextOverlay);DeleteImages(gNextSlides);
        gModelUpload=gCompassUpload=gOverlayUpload=gSlideUpload=0;gArtUploaded=false;
        gSlideshow.running=false;
        gGeneration=generation;gAnchor=gHead;gJobStarted=gUploaded=false;gVisibleStart=Fo3LoadingClockUs();gRotationStart=gVisibleStart;
    }
    PrepareCatalog();
    if(!gJobStarted && (gCatalog.Ready() || gCatalogFailed)) {
        const uint32_t cell=GetFo3LoadingCell(),world=GetFo3LoadingWorldspace();
        const uint64_t random=fo3loadingpose::Mix(gSeed^generation);
        if(gCatalog.Ready())gMenu=gCatalog.Get();
        const bool needCompass=gCompass.empty(),needOverlay=gOverlay.empty();
        const auto menu=gMenu;
        gJobStarted=gPreparation.Start([cell,world,random,needCompass,needOverlay,menu](CpuDisplay& out,const std::atomic<bool>& cancelled){
            const auto* art=fo3loading::Select(cell,world,random);
            if(art) {out.artPath=art->iconPath;LoadFalloutTextureRgba(art->iconPath,out.art);}
            size_t slideBytes=0;std::vector<std::string> paths;
            if(!out.art.rgba.empty()) {paths.push_back(out.artPath);out.slides.push_back(out.art);slideBytes=out.art.rgba.size();}
            for(size_t i=1;i<=32 && out.slides.size()<4 && !cancelled.load();++i) {
                const auto* choice=fo3loading::Select(cell,world,fo3loadingpose::Mix(random+i));
                if(!choice || std::find(paths.begin(),paths.end(),choice->iconPath)!=paths.end())continue;
                Fo3RgbaTexture image;
                if(LoadFalloutTextureRgba(choice->iconPath,image) && !image.rgba.empty() &&
                   slideBytes+image.rgba.size()<=16u*1024u*1024u) {
                    paths.push_back(choice->iconPath);slideBytes+=image.rgba.size();out.slides.push_back(std::move(image));
                }
            }
            if(needCompass) {
                ReadShapes(menu.compassPath,out.compass,cancelled,true);
                std::vector<uint8_t> bytes;
                if(LoadFalloutMeshFile(menu.compassPath,bytes,nullptr))
                    fo3anim::DecodeUiAnimation(bytes,out.compassHierarchy,out.compassBlocks,out.compassClips);
                for(auto& shape:out.compass)
                    if(shape.mesh.shapeBlock<out.compassBlocks.size())shape.bone=out.compassBlocks[shape.mesh.shapeBlock];
            }
            if(needOverlay) {
                ReadShapes(menu.overlayPath,out.overlay,cancelled,true);
                std::vector<uint8_t> bytes;
                if(LoadFalloutMeshFile(menu.overlayPath,bytes,nullptr) &&
                   fo3anim::DecodeUiAnimation(bytes,out.hierarchy,out.blockBones,out.clips)) {
                    fo3slideshow::Player check;
                    if(!check.Configure(out.hierarchy,out.clips))out.clips.clear();
                }
                if(out.clips.empty() || !PrepareOverlay(out)) {
                    // Show the resolved artwork when an unsupported NIF is
                    // installed; never leave static, off-screen slide layers.
                    out.overlay.clear();out.clips.clear();
                }
            }
            if(!fo3loading::gModels.empty()) {
                const size_t start=random%fo3loading::gModels.size();
                for(size_t i=0;i<std::min<size_t>(8,fo3loading::gModels.size()) && !cancelled.load();++i) {
                    const auto& path=fo3loading::gModels[(start+i)%fo3loading::gModels.size()];
                    out.model.clear();
                    if(ReadShapes(path,out.model,cancelled)) {out.modelPath=path;break;}
                }
            }
            fo3anim::Pose compassInitial;
            bool haveCompassPose=false;
            for(const auto& clip:out.compassClips)
                if(clip.name==menu.compassAnimation)
                    haveCompassPose=fo3anim::Sample(out.compassHierarchy,clip,clip.start,compassInitial);
            PrepareVertices(out.model,false);
            PrepareVertices(out.compass,true,0.17f,&out.compassToPanel,haveCompassPose?&compassInitial:nullptr);
            fo3anim::Inverse(out.compassToPanel,out.panelToCompass);
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
            if(gSlideUpload<cpu.slides.size()) {
                auto& image=cpu.slides[gSlideUpload++];GLuint texture=UploadImage(image);
                if(texture)gNextSlides.push_back(texture);
                image=Fo3RgbaTexture{};return;
            }
            if(gCompassUpload<cpu.compass.size()) {
                UploadShape(cpu.compass[gCompassUpload++],gNextCompass,true);return;
            }
            if(gOverlayUpload<cpu.overlay.size()) {
                UploadShape(cpu.overlay[gOverlayUpload++],gNextOverlay,true);return;
            }
            if(gModelUpload<cpu.model.size()) {
                UploadShape(cpu.model[gModelUpload++],gNextModel,false);return;
            }
            DeleteShapes(gModel);gModel.swap(gNextModel);
            if(!gNextCompass.empty()) {
                DeleteShapes(gCompass);gCompass.swap(gNextCompass);
                gCompassHierarchy=std::move(cpu.compassHierarchy);gCompassClips=std::move(cpu.compassClips);
                gCompassToPanel=cpu.compassToPanel;gPanelToCompass=cpu.panelToCompass;gCompassClip=-1;
                for(size_t i=0;i<gCompassClips.size();++i)
                    if(gCompassClips[i].name==gMenu.compassAnimation)gCompassClip=static_cast<int>(i);
                if(gCompassClip>=0)fo3anim::BindClip(gCompassHierarchy,gCompassClips[gCompassClip],gCompassPose);
                gCompassDeltas.resize(gCompassHierarchy.bones.size());
            }
            if(!gNextOverlay.empty()) {
                DeleteShapes(gOverlay);gOverlay.swap(gNextOverlay);
                gUiHierarchy=std::move(cpu.hierarchy);gUiClips=std::move(cpu.clips);
                gUiToPanel=cpu.uiToPanel;gPanelToUi=cpu.panelToUi;gUiHalfHeight=cpu.halfHeight;
                gUiDrawOrder.resize(gOverlay.size());
                for(size_t i=0;i<gUiDrawOrder.size();++i)gUiDrawOrder[i]=i;
                gUiDeltas.resize(gUiHierarchy.bones.size());
            }
            DeleteImages(gSlides);gSlides.swap(gNextSlides);
            gSlideshow.Configure(gUiHierarchy,gUiClips);
            gSlideStarted=Fo3LoadingClockUs();gSlideshow.Reset(0,gSlides.size());
            __android_log_print(ANDROID_LOG_INFO,"FalloutQuest",
                "VR LOADING ASSETS: generation=%llu art=%s model=%s compassShapes=%zu panel=2.5m exhibit=1.6m offset=(-0.58,-0.33)",
                static_cast<unsigned long long>(generation),cpu.artPath.c_str(),cpu.modelPath.c_str(),gCompass.size());
            __android_log_print(ANDROID_LOG_INFO,"FalloutQuest","ORIGINAL LOADING MENU: overlay=%s shapes=%zu mainMenuRGB=(%d,%d,%d) fade=%.2fs compassAnimation=%s controllers=embedded-transform-playback",
                gMenu.overlayPath.c_str(),gOverlay.size(),gMenu.red,gMenu.green,gMenu.blue,gMenu.fadeSeconds,gMenu.compassAnimation.c_str());
            __android_log_print(ANDROID_LOG_INFO,"FalloutQuest","LOADING SLIDESHOW: clips=%zu images=%zu canvasHeight=%.3f hold=6s active=%d",gUiClips.size(),gSlides.size(),gUiHalfHeight*2,gSlideshow.running);
            if(gOverlay.empty())__android_log_print(ANDROID_LOG_WARN,"FalloutQuest","AUTHORED LOADING OVERLAY UNAVAILABLE: %s; check original mesh/texture archives",gMenu.overlayPath.c_str());
            if(gCompass.empty())__android_log_print(ANDROID_LOG_WARN,"FalloutQuest","AUTHORED LOADING COMPASS UNAVAILABLE: Interface/Circular Loading/loading01.nif; check original mesh/texture archives");
        }
        gPreparation.Reset();gUploaded=true;gVisibleStart=Fo3LoadingClockUs();
    }
}
inline bool FullyVisible() {return gFrameSeconds>=gMenu.fadeSeconds;}
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
inline GLenum BlendFactor(uint8_t blend) {
    const GLenum factors[]={GL_ONE,GL_ZERO,GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR,GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR,
        GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA,GL_SRC_ALPHA_SATURATE};
    return blend<11?factors[blend]:GL_ONE;
}
inline void Draw(const Shape& s,const Matrix& mvp,float green=0,GLuint image=0) {
    glUniformMatrix4fv(gMvp,1,GL_FALSE,mvp.m);glUniform1f(gUnlit,s.unlit?1:0);
    glUniform4f(gTint,green>0?gMenu.red/255.0f:1,green>0?gMenu.green/255.0f:1,green>0?gMenu.blue/255.0f:1,s.alpha);
    glUniform1f(gCutoff,s.cutoff);
    if(s.blend) {glEnable(GL_BLEND);glBlendFunc(BlendFactor(s.sourceBlend),BlendFactor(s.destBlend));}
    else glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D,image?image:s.texture);glBindVertexArray(s.vao);
    glDrawElements(GL_TRIANGLES,s.count,GL_UNSIGNED_INT,nullptr);
}
inline void Render(GLuint framebuffer,GLsizei width,GLsizei height,const float* viewProjection,bool advance) {
    if(!IsFo3LoadingVisible() || width<=0 || height<=0)return;
    GLint program,vao,buffer,active,texture,readFb,drawFb,rbo,viewport[4],depthFunc,blend[4],equation[2];
    GLfloat clear[4];GLboolean mask,colourMask[4];
    glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFb);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFb);
    glGetIntegerv(GL_RENDERBUFFER_BINDING,&rbo);glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&equation[0]);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&equation[1]);
    glGetIntegerv(GL_BLEND_SRC_RGB,&blend[0]);glGetIntegerv(GL_BLEND_DST_RGB,&blend[1]);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&blend[2]);glGetIntegerv(GL_BLEND_DST_ALPHA,&blend[3]);
    glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);glGetBooleanv(GL_COLOR_WRITEMASK,colourMask);
    const GLenum caps[]={GL_DEPTH_TEST,GL_BLEND,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST};GLboolean enabled[5];
    for(int i=0;i<5;++i){enabled[i]=glIsEnabled(caps[i]);glDisable(caps[i]);}
    glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glClearColor(0,0,0,1);
    if(advance) {
        AdvanceAssets();
        const uint64_t now=Fo3LoadingClockUs();
        gFrameSeconds=gVisibleStart?static_cast<float>(now-gVisibleStart)/1000000:0;
        gFrameRotationSeconds=gRotationStart?static_cast<float>(now-gRotationStart)/1000000:0;
        if(gCompassClip>=0 && fo3anim::Sample(gCompassHierarchy,gCompassClips[gCompassClip],gFrameRotationSeconds,gCompassPose))
            for(size_t i=0;i<gCompassDeltas.size();++i) {
                auto delta=fo3anim::Multiply(gCompassToPanel,fo3anim::Multiply(gCompassPose.delta[i],gPanelToCompass));
                std::copy(delta.begin(),delta.end(),gCompassDeltas[i].m);
            }
        if(gSlideshow.Advance(gUiHierarchy,gUiClips,gSlideStarted?double(now-gSlideStarted)/1000000:0,gSlides.size())) {
            for(size_t i=0;i<gUiDeltas.size();++i) {
                auto delta=fo3anim::Multiply(gUiToPanel,fo3anim::Multiply(gSlideshow.pose.delta[i],gPanelToUi));
                std::copy(delta.begin(),delta.end(),gUiDeltas[i].m);
            }
            auto depth=[](const Shape& shape) {
                if(shape.bone<0 || static_cast<size_t>(shape.bone)>=gUiDeltas.size())return shape.centre[2];
                const auto& m=gUiDeltas[shape.bone].m;
                return m[2]*shape.centre[0]+m[6]*shape.centre[1]+m[10]*shape.centre[2]+m[14];
            };
            std::stable_sort(gUiDrawOrder.begin(),gUiDrawOrder.end(),[&](size_t a,size_t b){return depth(gOverlay[a])<depth(gOverlay[b]);});
            for(const auto& sound:gSlideshow.sounds)fo3audio::NamedSound(sound);
        }
    }
    const bool target=EnsureTarget(width,height);
    if(target) {
        glViewport(0,0,width,height);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        if(EnsureProgram()) {
            Matrix vp;std::copy(viewProjection,viewProjection+16,vp.m);
            Matrix panel=Multiply(vp,gAnchor);
            glUseProgram(gProgram);glUniform1i(gSampler,0);
            const float seconds=gFrameSeconds;
            glUniform1f(gFade,std::clamp(seconds/gMenu.fadeSeconds,0.0f,1.0f));
            glUniform1f(gClipUi,0);const Matrix identity=fo3loadingpose::Identity();glUniformMatrix4fv(gUiTransform,1,GL_FALSE,identity.m);
            glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);
            if(gArt) {
                Matrix size=fo3loadingpose::Identity();size.m[0]=1.2f;size.m[5]=1.2f/gArtAspect;
                Matrix mvp=Multiply(panel,size);glUniformMatrix4fv(gMvp,1,GL_FALSE,mvp.m);
                glUniform1f(gUnlit,1);glUniform1f(gCutoff,0.02f);
                glUniform4f(gTint,gMenu.red/255.0f,gMenu.green/255.0f,gMenu.blue/255.0f,1);glBindTexture(GL_TEXTURE_2D,gArt);glBindVertexArray(gQuad);
                glDrawArrays(GL_TRIANGLE_STRIP,0,4);
            }
            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            // loading_nif explicitly opts out of inherited system colour.
            glDepthMask(GL_FALSE);
            const Matrix overlay=Multiply(panel,Placement(0,0,0.02f,0));
            if(gSlideshow.running) {
                glUniform1f(gClipUi,1);glUniform2f(gClipHalf,1.2f,gUiHalfHeight);
                for(size_t index:gUiDrawOrder) {
                    const auto& shape=gOverlay[index];
                    const Matrix& delta=shape.bone>=0 && static_cast<size_t>(shape.bone)<gUiDeltas.size()?gUiDeltas[shape.bone]:identity;
                    glUniformMatrix4fv(gUiTransform,1,GL_FALSE,delta.m);
                    GLuint texture=0;
                    if(shape.slide>=0 && !gSlides.empty())texture=gSlides[gSlideshow.slots[shape.slide]%gSlides.size()];
                    Draw(shape,Multiply(overlay,delta),0,texture);
                }
                glUniform1f(gClipUi,0);glUniformMatrix4fv(gUiTransform,1,GL_FALSE,identity.m);
            }
            glDepthMask(GL_TRUE);
            Matrix model=Multiply(panel,Placement(fo3loadingpose::ModelX,fo3loadingpose::ModelY,fo3loadingpose::PanelDistance-fo3loadingpose::ModelDistance,gFrameRotationSeconds*fo3loadingpose::RotationRadiansPerSecond));
            for(const auto& s:gModel)Draw(s,model);
            // The authored Idle sequence animates Pointer independently of
            // the dial. Unsupported controller data leaves the dial stationary.
            Matrix compass=Multiply(panel,Placement(1.04f,-(gSlideshow.running?gUiHalfHeight:1.2f/gArtAspect)+0.14f,0.10f,0));
            glDisable(GL_DEPTH_TEST);
            for(const auto& shape:gCompass) {
                const Matrix& delta=gCompassClip>=0 && shape.bone>=0 && static_cast<size_t>(shape.bone)<gCompassDeltas.size()?gCompassDeltas[shape.bone]:identity;
                Draw(shape,Multiply(compass,delta),1);
            }
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER,gFramebuffer);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffer);
        glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    } else {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffer);glClear(GL_COLOR_BUFFER_BIT);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER,readFb);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFb);glBindRenderbuffer(GL_RENDERBUFFER,rbo);
    glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glClearColor(clear[0],clear[1],clear[2],clear[3]);
    glColorMask(colourMask[0],colourMask[1],colourMask[2],colourMask[3]);glDepthMask(mask);glDepthFunc(depthFunc);
    glBlendFuncSeparate(blend[0],blend[1],blend[2],blend[3]);glBlendEquationSeparate(equation[0],equation[1]);
    for(int i=0;i<5;++i)if(enabled[i])glEnable(caps[i]);else glDisable(caps[i]);
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);glUseProgram(program);
    glBindTexture(GL_TEXTURE_2D,texture);glActiveTexture(active);
}
inline void Shutdown() {
    gPreparation.Reset();gCatalog.Reset();
    fo3loading::gScreens.clear();fo3loading::gModels.clear();fo3loading::gScreensPrepared=false;
    gMenu=fo3loadingmenu::Definition{};
    gCatalogStarted=gCatalogFailed=false;gJobStarted=gUploaded=false;
    DeleteShapes(gModel);DeleteShapes(gCompass);DeleteShapes(gOverlay);DeleteShapes(gNextModel);DeleteShapes(gNextCompass);DeleteShapes(gNextOverlay);DeleteImages(gSlides);DeleteImages(gNextSlides);
    gUiHierarchy={};gUiClips.clear();gSlideshow={};gUiDeltas.clear();gUiDrawOrder.clear();
    gCompassHierarchy={};gCompassClips.clear();gCompassPose={};gCompassClip=-1;gCompassDeltas.clear();
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
