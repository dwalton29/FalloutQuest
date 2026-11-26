// Batched world-space billboards. Geometry and texture inputs come from the
// active explosion's original NIF. Game-space positions survive origin shifts.
#include "rendering/mesh/fo3-burst-particles.h"
namespace fo3worldfx {
struct Particle { std::array<float,3> start{},velocity{};float birth=0,radius=0,life=0; };
struct Batch { fo3burst::Layer layer;std::vector<Particle> particles; };
struct Burst { uint32_t cell=0,world=0;double start=0;std::vector<Batch> batches; };
static std::vector<Burst> bursts;
static std::unordered_map<std::string,std::vector<fo3burst::Layer>> models;
static std::unordered_map<std::string,GLuint> textures;
static GLuint vao=0,vbo=0,shader=0;
static GLint mvpLocation=-1,tintLocation=-1,textureLocation=-1;
static double clock=0;
static uint32_t serial=0;
static bool SameArea(uint32_t cell,uint32_t world){
    return world==gExteriorWorldspaceQ1890&&(world||cell==gCurrentCellFormId);
}
static void Start(const fo3script::Action& a,const fo3player::ExplosionDefinition& e,double now){
    if(!SameArea(a.cell,a.world))return;
    if(e.force>0&&e.radius>0){
        const fo3rigid::V center{(a.position[0]-gSceneCenterXQ1730)/FO3_UNITS_PER_METRE,
          FLOOR_Y+(a.position[2]-gSceneFloorZQ1730)/FO3_UNITS_PER_METRE,
          SCENE_FORWARD-(a.position[1]-gSceneCenterYQ1730)/FO3_UNITS_PER_METRE};
        fo3rigid::Live().Blast(center,e.radius/FO3_UNITS_PER_METRE,e.force/FO3_UNITS_PER_METRE);
    }
    if(e.model.empty())return;
    const auto inserted=models.try_emplace(e.model);
    if(inserted.second){
        std::vector<uint8_t> bytes;
        if(LoadFalloutMeshFile(e.model,bytes,nullptr))inserted.first->second=fo3burst::Decode(bytes);
    }
    Burst burst;burst.cell=a.cell;burst.world=a.world;burst.start=now;
    std::mt19937 rng(++serial^a.form^a.reference);
    std::uniform_real_distribution<float> unit(0,1);
    auto signedUnit=[&](){return 2.f*unit(rng)-1.f;};
    for(const auto& layer:inserted.first->second){
        Batch batch;batch.layer=layer;batch.particles.reserve(layer.count);
        for(uint32_t i=0;i<layer.count;++i){
            Particle p;p.start=a.position;
            std::array<float,3> v{};
            do {for(auto& n:v)n=signedUnit();}while(layer.sphere&&v[0]*v[0]+v[1]*v[1]+v[2]*v[2]>1);
            for(size_t n=0;n<3;++n)v[n]*=layer.volume[n]*(layer.sphere?1.f:.5f);
            const auto& transform=layer.emitterToModel;
            fo3_q6f_collision_detail::TransformPoint(transform,v[0],v[1],v[2]);
            for(size_t n=0;n<3;++n)p.start[n]+=v[n];
            const float d=layer.declination+signedUnit()*layer.declinationVariation;
            const float theta=layer.angle+signedUnit()*layer.angleVariation;
            const float speed=layer.speed+.5f*signedUnit()*layer.speedVariation;
            const std::array<float,3> localVelocity{speed*std::sin(d)*std::cos(theta),speed*std::sin(d)*std::sin(theta),speed*std::cos(d)};
            for(size_t axis=0;axis<3;++axis){
                const float* row=transform.m+4*axis;
                p.velocity[axis]=row[0]*localVelocity[0]+row[1]*localVelocity[1]+row[2]*localVelocity[2];
            }
            p.birth=layer.begin+layer.window*(float(i)+.5f)/layer.count;
            p.radius=std::max(0.f,layer.radius+.5f*signedUnit()*layer.radiusVariation);
            p.life=std::max(0.f,layer.life+.5f*signedUnit()*layer.lifeVariation);
            if(p.life>0&&p.radius>0)batch.particles.push_back(p);
        }
        if(!batch.particles.empty())burst.batches.push_back(std::move(batch));
    }
    if(!burst.batches.empty())bursts.push_back(std::move(burst));
}
static void Advance(double now){
    clock=now;
    bursts.erase(std::remove_if(bursts.begin(),bursts.end(),[&](const Burst& b){
        if(!SameArea(b.cell,b.world))return true;
        for(const auto& batch:b.batches)for(const auto& p:batch.particles)
            if(now-b.start<p.birth+p.life)return false;
        return true;
    }),bursts.end());
}
static bool RefreshModels(fo3player::Player& player,const std::unordered_set<uint32_t>& refs){
    const auto state=std::make_shared<const fo3world::ReferenceStateSnapshot>(player.CaptureReferenceState());
    bool resident=false;
    for(const auto& obj:gObjects){
        if(!refs.count(obj.refFormId))continue;
        const auto desired=state->modelOverrides.find(obj.refFormId);
        if(desired!=state->modelOverrides.end()&&fo3script::Lower(desired->second)!=fo3script::Lower(obj.modelPath)){
            resident=true;break;
        }
    }
    // Exterior cells replace their own render and collision data off-thread.
    // Keep the old shapes visible until their replacement has finished upload.
    if(resident&&!gExteriorStreamingActiveQ1890&&!QueueFo3WorldModelRefresh(gCurrentCellFormId,gExteriorWorldspaceQ1890,
           gSceneCenterXQ1730,gSceneCenterYQ1730,gSceneFloorZQ1730))return false;
    gFo3ReferenceState=state;gFo3ScenePublication.Publish(state);return true;
}
static void Clear(){bursts.clear();clock=0;}
static void Shutdown(){
    Clear();models.clear();
    if(shader)glDeleteProgram(shader);shader=0;
    for(const auto& t:textures)if(t.second)glDeleteTextures(1,&t.second);
    textures.clear();if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);vbo=vao=0;
}
static void Draw(const float* mvp){
    if(bursts.empty())return;
    fo3hudrenderer::CachedStateGuard guard;
    if(!shader){
        shader=fo3hudassets::BuildProgram();
        if(!shader)return;
        mvpLocation=glGetUniformLocation(shader,"uMvp");
        tintLocation=glGetUniformLocation(shader,"uTint");
        textureLocation=glGetUniformLocation(shader,"uTex");
    }
    if(!vao){glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);}
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    using Vertex=fo3hudassets::Vertex;
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(3*sizeof(float)));
    glEnable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);
    glUseProgram(shader);glUniformMatrix4fv(mvpLocation,1,GL_FALSE,mvp);
    glUniform1i(textureLocation,0);glActiveTexture(GL_TEXTURE0);
    for(const auto& burst:bursts)for(const auto& batch:burst.batches){
        const auto& l=batch.layer;const auto entry=textures.try_emplace(l.texture,0);
        if(entry.second){Fo3RgbaTexture t;if(LoadFalloutTextureRgba(l.texture,t))
            entry.first->second=fo3hudassets::UploadTexture(t.width,t.height,t.rgba.data());}
        if(!entry.first->second)continue;
        std::array<std::vector<Vertex>,8> fades;
        for(const auto& p:batch.particles){
            const float age=float(clock-burst.start)-p.birth;
            if(age<0||age>=p.life)continue;
            const float fraction=age/p.life;
            float opacity=1;
            if(l.fadeIn>0&&fraction<l.fadeIn)opacity=fraction/l.fadeIn;
            if(l.fadeOut<1&&fraction>l.fadeOut)opacity=(1-fraction)/(1-l.fadeOut);
            float r=p.radius/FO3_UNITS_PER_METRE;
            if(l.grow>0)r*=std::min(1.f,age/l.grow);
            if(l.shrink>0)r*=std::min(1.f,(p.life-age)/l.shrink);
            const float x=(p.start[0]+p.velocity[0]*age-gSceneCenterXQ1730)/FO3_UNITS_PER_METRE;
            const float y=FLOOR_Y+(p.start[2]+p.velocity[2]*age-gSceneFloorZQ1730)/FO3_UNITS_PER_METRE;
            const float z=SCENE_FORWARD-(p.start[1]+p.velocity[1]*age-gSceneCenterYQ1730)/FO3_UNITS_PER_METRE;
            const float dx=gQ210Head[0]-x,dy=gQ210Head[1]-y,dz=gQ210Head[2]-z;
            const float planar=std::hypot(dx,dz),distance=std::hypot(planar,dy);
            if(planar<.001f||distance<.001f||opacity<=0)continue;
            const float rx=dz/planar,rz=-dx/planar,ux=-dx*dy/(planar*distance),uy=planar/distance,uz=-dz*dy/(planar*distance);
            auto& vertices=fades[std::clamp(int(opacity*8),0,7)];
            auto corner=[&](float h,float v){vertices.push_back({x+r*(rx*h+ux*v),y+r*uy*v,z+r*(rz*h+uz*v),(h+1)*.5f,(1-v)*.5f});};
            corner(-1,-1);corner(1,-1);corner(1,1);corner(-1,-1);corner(1,1);corner(-1,1);
        }
        glBindTexture(GL_TEXTURE_2D,entry.first->second);
        glBlendFunc(Q1150BlendFactor(l.sourceBlend,true),Q1150BlendFactor(l.destBlend,false));
        for(size_t i=0;i<fades.size();++i){
            const auto& vertices=fades[i];if(vertices.empty())continue;
            glUniform4f(tintLocation,1,1,1,float(i+1)/8);
            glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size()));
        }
    }
}
}
