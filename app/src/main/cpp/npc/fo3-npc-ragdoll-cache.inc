// Scene worker: decode the original rig once per skeleton in this batch.
static void Q230CacheRagdoll(Q230ActorVisual& actor,
 std::unordered_map<std::string,std::shared_ptr<const fo3rag::Definition>>& cache,
 const fo3player::PagedStateMap<fo3player::ActorState>* saved){
 if(actor.source.creature)return; // Creature constraints require a separate asset audit.
 // Initial globals/local attachment transforms must exist even before the
 // first visible draw, including on restored corpses that bypass animation.
 fo3anim::BindClip(actor.skeleton,actor.clip,actor.pose);
 if(!fo3anim::Sample(actor.skeleton,actor.clip,0,actor.pose))return;
 const auto& path=actor.source.skeletonModel;
 auto found=cache.find(path);
 if(found==cache.end()){
  std::vector<uint8_t> bytes;auto definition=std::make_shared<fo3rag::Definition>();std::string error;
  if(!LoadFalloutMeshFile(path,bytes)||!fo3rag::Read(bytes,*definition,error)){
   Q6H_LOGI("NPC RAGDOLL UNAVAILABLE model=%s reason=%s",path.c_str(),error.c_str());definition.reset();
  }else Q6H_LOGI("NPC RAGDOLL ASSET model=%s bodies=%zu joints=%zu",path.c_str(),definition->bodies.size(),definition->joints.size());
  found=cache.emplace(path,std::move(definition)).first;
 }
 if(!found->second)return;
 for(const auto& body:found->second->bodies){const int bone=fo3anim::FindBone(actor.skeleton,body.bone);if(bone<0){actor.ragdollBones.clear();return;}actor.ragdollBones.push_back(bone);}
 actor.ragdollDefinition=found->second;
 if(saved){const auto state=saved->find(actor.source.refFormId);
  if(state!=saved->end()&&state->second.dead&&state->second.corpse&&state->second.corpse->model==path){
   const auto& corpse=*state->second.corpse;bool matches=corpse.bones.size()==actor.ragdollBones.size();
   for(size_t i=0;matches&&i<corpse.bones.size();++i)matches=corpse.bones[i].name==actor.ragdollDefinition->bodies[i].bone;
   if(matches&&fo3rag::ApplyPose(actor.skeleton,actor.pose,corpse)){actor.corpsePose=state->second.corpse;actor.ragdollStarted=true;}
  }
 }
}
