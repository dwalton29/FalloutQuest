// Cached NIF resources are shared by bones, actors and both eyes. Placement
// uses the sampled actor or saved ragdoll transform at draw time.
namespace fo3addonrenderer {
struct Model {
    fo3dooranim::Asset animation;
    std::vector<GpuObject> shapes;
    std::vector<float> opacity;
    fo3anim::Pose pose;
    fo3anim::Matrix inverseUpload=fo3anim::Identity();
    std::vector<fo3anim::Matrix> corrections;
    float sampledAt=-1;
};
static std::unordered_map<std::string,Model> addonModels;
static void Shutdown(){
    for(auto& model:addonModels)Q74DeleteGpuObjects(model.second.shapes);
    addonModels.clear();
}
static Model& Resource(const std::string& path){
    auto [it,inserted]=addonModels.try_emplace(path);
    auto& model=it->second;
    if(!inserted)return model;
    Fo3WorldPlacement source{};source.scale=1;source.modelPath=path;source.baseRecordType="DEBR";
    std::vector<CpuObject> parts;
    if(!BuildCpuObjects(source,parts,true))return model;
    std::vector<uint8_t> bytes;
    if(!LoadFalloutMeshFile(path,bytes,nullptr))return model;
    auto& asset=model.animation;
    fo3anim::DecodeObjectAnimation(bytes,asset.hierarchy,asset.blockBones,asset.clips,true);
    fo3anim::Matrix upload;Q2016BuildPlacementMatrix(source,0,0,0,upload.data());
    for(int col=0;col<3;++col)for(int row=0;row<3;++row)upload[col*4+row]/=FO3_UNITS_PER_METRE;
    if(!fo3anim::Inverse(upload,model.inverseUpload))return model;
    for(auto& part:parts){
        const auto block=part.mesh.shapeBlock;
        if(part.mesh.diffuseTexturePath.empty())continue;
        GpuObject gpu;
        if(!UploadCpuObject(part,0,0,0,gpu))continue;
        gpu.scriptAnimated=true;
        gpu.q2401AnimationBone=block<asset.blockBones.size()?asset.blockBones[block]:-1;
        model.opacity.push_back(gpu.materialAlpha);
        model.shapes.push_back(std::move(gpu));
    }
    return model;
}
static void DrawAddons(){
    if(!gPlayerSession)return;
    const auto& player=gPlayerSession->player;
    if(player.Snapshot().magicShaders.empty())return;
    const auto& data=player.Definitions().pipboy;
    fo3hudrenderer::CachedStateGuard guard;
    const auto previous=gQ230DetailedObjectPrevalidated;
    gQ230DetailedObjectPrevalidated=true;
    for(const auto& [key,play]:player.Snapshot().magicShaders){
        const auto definition=data.effectShaders.find(key.second);
        if(definition==data.effectShaders.end()||!definition->second.addon)continue;
        const auto variants=data.debrisModels.find(definition->second.addon);
        if(variants==data.debrisModels.end())continue;
        for(const auto& actor:gQ230NpcActors){
            if(actor.source.refFormId!=key.first||actor.runtime.offScene||actor.rigs.empty()||
               !actor.renderVisible||Fo3WorldReferenceHidden(key.first))continue;
            const auto scene=fo3anim::Multiply(actor.dialogueRoot,actor.rigs.front().scenePlacement);
            fo3actorfx::VisitAddons(actor.skeleton,actor.pose,actor.corpsePose.get(),scene,definition->second,
                        variants->second,play,[&](size_t node,const auto& variant,const auto& placement,float alpha){
                if(alpha<=0)return;
                auto& m=Resource(variant.path);
                const auto& a=m.animation;
                const bool animated=a.clips.size()==1&&a.clips.front().DirectControllers();
                if(animated){
                    if(m.sampledAt!=play.elapsed){
                        if(!fo3anim::Sample(a.hierarchy,a.clips.front(),play.elapsed,m.pose))return;
                        m.sampledAt=play.elapsed;
                    }
                    fo3dooranim::BillboardCorrections(a,m.pose,placement.data(),1,gQ210Head,m.corrections);
                }
                for(size_t i=0;i<m.shapes.size();++i){
                    auto& shape=m.shapes[i];
                    if(!shape.realDiffuse)continue;
                    const int bone=shape.q2401AnimationBone;
                    auto transform=placement;
                    if(animated&&bone>=0){
                        transform=fo3anim::Multiply(m.corrections[bone],fo3anim::Multiply(placement,m.pose.delta[bone]));
                        fo3anim::DiffuseRowsForBone(m.pose,bone,shape.scriptDiffuseRows);
                    }
                    transform=fo3anim::Multiply(transform,m.inverseUpload);
                    std::copy(transform.begin(),transform.end(),shape.q2400DoorTransform);
                    shape.materialAlpha=m.opacity[i]*alpha;
                    if(shape.zBufferTestQ1200)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
                    glDepthMask(shape.zBufferWriteQ1200?GL_TRUE:GL_FALSE);
                    glEnable(GL_BLEND);glBlendFunc(Q1150BlendFactor(shape.alphaSourceBlend,true),Q1150BlendFactor(shape.alphaDestBlend,false));
                    const uint64_t instance=(uint64_t(key.first)<<32)|(key.second^uint32_t(node*2654435761u));
                    DrawSceneObject(shape,false,{fqspacewarp::PlacementKind::ShaderAddon,instance});
                }
            });
        }
    }
    gQ230DetailedObjectPrevalidated=previous;
}
}
