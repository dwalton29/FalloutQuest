// Included after GpuObject. Bind-pose mesh buffers never change in GPU mode.
GLint gActorSkinMode=-1, gActorPalette=-1;
struct QActorSkin {
    fqskin::Mapping mapping;
    std::vector<fqskin::PaletteRow> palette;
    std::vector<float> bind, work; // CPU reference/fallback only
    std::vector<fqskin::PaletteRow> cpuMatrices;
};
namespace fqactor {
struct Cost {
    double pose=0,fingers=0,bones=0,skin=0,directions=0,upload=0,draw=0,clip=0;
    uint64_t vertices=0,parts=0,actors=0,vertexBytes=0,vboUploads=0,paletteBytes=0,gpuVertices=0,referenceBytes=0;
};
inline Cost player,npc;
inline fqopaque::Average framePrep,frameWork,frameCadence;
struct Rolling {
    fqopaque::Average pose,fingers,bones,clip,skin,directions,upload,draw;
    void Add(const Cost& c) { pose.Add(c.pose);fingers.Add(c.fingers);bones.Add(c.bones);clip.Add(c.clip);skin.Add(c.skin);directions.Add(c.directions);upload.Add(c.upload);draw.Add(c.draw); }
};
inline Rolling playerRolling,npcRolling;
inline fqopaque::Clock::time_point lastFrame{};
inline bool CpuReference() {
    static const bool enabled=[] {
#ifndef NDEBUG
        char value[PROP_VALUE_MAX]{};__system_property_get("debug.falloutquest.cpu_skin",value);
        return std::atoi(value)==1;
#else
        return false;
#endif
    }();return enabled;
}
}
void QActorDeleteSkin(GpuObject& object) {
    if(object.skinAttributes) glDeleteBuffers(1,&object.skinAttributes);
    if(object.skinPalette) glDeleteTextures(1,&object.skinPalette);
    object.skinAttributes=object.skinPalette=0;
}
bool QActorCreateSkin(GpuObject& object,QActorSkin& skin,
                      const std::vector<uint16_t>& indices,const std::vector<float>& weights,
                      size_t bones,bool player,const std::vector<float>& bind) {
    skin.mapping=fqskin::Map(indices,weights,bones,player);
    if(skin.mapping.attributes.size()!=static_cast<size_t>(object.vertexCount)*8) return false;
    skin.palette.resize(skin.mapping.sources.size()+1);
    for(auto& row:skin.palette) fqskin::Pack(row,fqskin::Identity(),fqskin::Identity());
    GLint units=0,size=0,attributes=0;
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,&units);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,&size);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,&attributes);
    const bool supported=units>0 && size>=8 && skin.palette.size()<=static_cast<size_t>(size) && attributes>=12;
    object.skinMode=player?1:2;
    if(!supported || fqactor::CpuReference()) {
        object.skinMode=0;skin.bind=bind;skin.work=bind;
        skin.cpuMatrices.resize(static_cast<size_t>(object.vertexCount));
        Q6H_LOGW("ACTOR SKIN CPU REFERENCE supported=%d requested=%d model=%s bones=%zu vertexUnits=%d maxTexture=%d maxAttribs=%d",
            supported,fqactor::CpuReference(),object.modelPath.c_str(),skin.palette.size(),units,size,attributes);
        return true;
    }
    glGenBuffers(1,&object.skinAttributes);
    glBindVertexArray(object.vao);
    glBindBuffer(GL_ARRAY_BUFFER,object.skinAttributes);
    glBufferData(GL_ARRAY_BUFFER,skin.mapping.attributes.size()*sizeof(float),skin.mapping.attributes.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(10,4,GL_FLOAT,GL_FALSE,8*sizeof(float),nullptr);glEnableVertexAttribArray(10);
    glVertexAttribPointer(11,4,GL_FLOAT,GL_FALSE,8*sizeof(float),reinterpret_cast<void*>(4*sizeof(float)));glEnableVertexAttribArray(11);
    glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);
    glGenTextures(1,&object.skinPalette);glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D,object.skinPalette);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,8,skin.palette.size(),0,GL_RGBA,GL_FLOAT,skin.palette.data());
    glActiveTexture(GL_TEXTURE0);
    Q6H_LOGI("ACTOR SKIN GPU model=%s meshBones=%zu paletteBones=%zu vertices=%d vertexUnits=%d maxTexture=%d maxAttribs=%d positionAndDirectionPalettes=1",
        object.modelPath.c_str(),bones,skin.palette.size()-1,object.vertexCount,units,size,attributes);
    return true;
}
void QActorUploadSkin(GpuObject& object,QActorSkin& skin,fqactor::Cost& cost,bool player) {
    if(object.skinMode) {
        const auto started=fqopaque::Clock::now();
        glActiveTexture(GL_TEXTURE6);glBindTexture(GL_TEXTURE_2D,object.skinPalette);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,8,skin.palette.size(),GL_RGBA,GL_FLOAT,skin.palette.data());
        glActiveTexture(GL_TEXTURE0);
        cost.upload+=fqopaque::Micros(started);cost.paletteBytes+=skin.palette.size()*sizeof(fqskin::PaletteRow);
        cost.gpuVertices+=object.vertexCount;
    } else {
        // Diagnostic/unsupported-device reference, separately timed at mesh scope.
        auto started=fqopaque::Clock::now();
        for(size_t v=0;v<skin.cpuMatrices.size();++v) {
            skin.cpuMatrices[v]=fqskin::Blend(skin.mapping.attributes.data()+v*8,skin.palette,player);
            const auto p=fqskin::Transform(skin.cpuMatrices[v].data(),skin.bind.data()+v*18,false);
            std::copy(p.begin(),p.end(),skin.work.begin()+v*18);
        }
        cost.skin+=fqopaque::Micros(started);started=fqopaque::Clock::now();
        for(size_t v=0;v<skin.cpuMatrices.size();++v) for(size_t component=1;component<4;++component) {
            const float* bind=skin.bind.data()+v*18+component*3;
            auto d=fqskin::Transform(skin.cpuMatrices[v].data()+16,bind,true);
            const float len=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
            for(size_t k=0;k<3;++k) skin.work[v*18+component*3+k]=(player ? len>1e-6f : len>=1e-7f) ? d[k]/len : (player ? bind[k] : (k==2 ? -1.0f : 0.0f));
        }
        cost.directions+=fqopaque::Micros(started);started=fqopaque::Clock::now();
        glBindBuffer(GL_ARRAY_BUFFER,object.vbo);
        glBufferSubData(GL_ARRAY_BUFFER,0,skin.work.size()*sizeof(float),skin.work.data());
        glBindBuffer(GL_ARRAY_BUFFER,0);
        cost.upload+=fqopaque::Micros(started);cost.vertexBytes+=skin.work.size()*sizeof(float);++cost.vboUploads;
        cost.vertices+=object.vertexCount;
    }
    cost.referenceBytes+=static_cast<uint64_t>(object.vertexCount)*18*sizeof(float);
    ++cost.parts;
}
