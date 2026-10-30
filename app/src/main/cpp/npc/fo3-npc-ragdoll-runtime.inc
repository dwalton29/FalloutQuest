// Render-thread ownership. One shared physics step and one immutable pose for
// both eyes/SpaceWarp; draws never advance simulation or write player state.
static void Q230TrackRagdollMotion(Q230ActorVisual& actor,double now){
 if(actor.ragdollStarted)return;const float dt=float(now-actor.ragdollMotionAt);auto velocity=fo3rigid::V{};
 if(actor.ragdollMotionAt>=0&&dt>.001f&&dt<.1f){float distance=0;for(int k=0;k<3;++k){const float d=actor.runtime.position[k]-actor.ragdollPreviousPosition[k];distance+=d*d;velocity[k]=d/dt;}if(distance>1)velocity={};}
 actor.ragdollVelocity=velocity;actor.ragdollPreviousPosition=actor.runtime.position;actor.ragdollMotionAt=now;
}
static void Q230StartRagdoll(Q230ActorVisual& actor){
 if(!gPlayerSession||!actor.stateRestored||actor.runtime.offScene||actor.rigs.empty()||!actor.ragdollDefinition)return;
 const auto id=actor.source.refFormId;auto& player=gPlayerSession->player;auto& world=fo3rigid::Live();
 if(player.ActorHealth(id)!=0||player.Essential(id)){
  if(actor.ragdollStarted){world.RemoveRagdoll(id);actor.ragdollStarted=false;actor.corpsePose.reset();actor.lastFrame=UINT64_MAX;}return;
 }
 if(world.HasRagdoll(id))return;
 // Use the last animated pose on the death edge, before Die selects another
 // clip. Restored corpses already have their physical global bone transforms.
 if(actor.pose.global.size()!=actor.skeleton.bones.size())return;
 auto root=fo3anim::Identity();const float yaw=actor.runtime.yaw-actor.runtime.authoredYaw;
 root[0]=root[10]=std::cos(yaw);root[2]=-std::sin(yaw);root[8]=-root[2];
 const auto& placement=actor.rigs[0].scenePlacement;
 root[12]=actor.runtime.position[0]-root[0]*placement[12]-root[8]*placement[14];root[13]=actor.runtime.position[1]-placement[13];root[14]=actor.runtime.position[2]-root[2]*placement[12]-root[10]*placement[14];
 actor.dialogueRoot=root;const auto scene=fo3anim::Multiply(root,placement);
 float scale=0;for(int r=0;r<3;++r)scale+=scene[r]*scene[r];scale=std::sqrt(scale);
 actor.ragdollScales.clear();
 std::vector<fo3rigid::M> bones;for(size_t i=0;i<actor.ragdollBones.size();++i){
  const auto& global=actor.corpsePose?actor.corpsePose->bones[i].global:actor.pose.global[actor.ragdollBones[i]];
  auto transform=fo3anim::Multiply(scene,global);float length=0;for(int r=0;r<3;++r)length+=transform[r]*transform[r];actor.ragdollScales.push_back(std::sqrt(length));if(!fo3rag::Rigid(transform))return;bones.push_back(transform);
 }
 const bool restored=bool(actor.corpsePose);
 if(!world.AddRagdoll(id,*actor.ragdollDefinition,bones,scale,actor.ragdollVelocity,actor.ragdollScales))return;
 if(actor.corpsePose){auto rotation=scene;if(!fo3rag::Rigid(rotation))return;std::vector<fo3rigid::V> linear,angular;for(const auto& bone:actor.corpsePose->bones){linear.push_back(fo3anim::Point(scene,bone.linear,true));angular.push_back(fo3anim::Point(rotation,bone.angular,true));}world.SetRagdollMotion(id,linear,angular);if(actor.corpsePose->sleeping)world.SleepRagdoll(id);}
 if(!actor.corpsePose){fo3player::CorpsePose initial;initial.model=actor.source.skeletonModel;fo3anim::Matrix inverse;if(!fo3anim::Inverse(scene,inverse))return;const auto velocity=fo3anim::Point(inverse,actor.ragdollVelocity,true);for(size_t i=0;i<actor.ragdollBones.size();++i)initial.bones.push_back({actor.ragdollDefinition->bodies[i].bone,actor.pose.global[actor.ragdollBones[i]],velocity,{}});actor.corpsePose=std::make_shared<const fo3player::CorpsePose>(std::move(initial));}
 actor.ragdollStarted=true;actor.lastFrame=UINT64_MAX;
 player.SetCorpsePose(id,*actor.corpsePose);
 Q6H_LOGI("NPC RAGDOLL START actor=%08X restored=%d bodies=%zu",id,restored,bones.size());
}
void Q230PrepareRagdolls(){
 auto& world=fo3rigid::Live();
 for(auto id:world.RagdollActors())if(std::none_of(gQ230NpcActors.begin(),gQ230NpcActors.end(),[&](const auto& a){return a.source.refFormId==id;}))world.RemoveRagdoll(id);
 for(auto& actor:gQ230NpcActors)Q230StartRagdoll(actor);
}
void Q230CaptureRagdolls(){
 if(!gPlayerSession)return;
 for(auto& actor:gQ230NpcActors){
  if(!actor.ragdollStarted||actor.rigs.empty())continue;
  // Check activation before allocating/copying 18 body transforms and velocities.
  if(actor.corpsePose&&actor.corpsePose->sleeping&&fo3rigid::Live().RagdollSleeping(actor.source.refFormId))continue;
  std::vector<fo3rigid::M> bones;std::vector<fo3rigid::V> linear,angular;bool sleeping=false;
  if(!fo3rigid::Live().RagdollPose(actor.source.refFormId,bones,&sleeping,&linear,&angular))continue;
  if(sleeping&&actor.corpsePose&&actor.corpsePose->sleeping)continue;
  const auto scene=fo3anim::Multiply(actor.dialogueRoot,actor.rigs[0].scenePlacement);fo3anim::Matrix inverse;
  if(!fo3anim::Inverse(scene,inverse))continue;
  auto inverseRotation=inverse;if(!fo3rag::Rigid(inverseRotation))continue;
  float sceneScale=0;for(int r=0;r<3;++r)sceneScale+=scene[r]*scene[r];sceneScale=std::sqrt(sceneScale);
  fo3player::CorpsePose corpse;corpse.model=actor.source.skeletonModel;corpse.sleeping=sleeping;
  for(size_t i=0;i<bones.size();++i){auto global=fo3anim::Multiply(inverse,bones[i]);if(!fo3rag::Rigid(global)){corpse.bones.clear();break;}for(int c=0;c<3;++c)for(int r=0;r<3;++r)global[c*4+r]*=actor.ragdollScales[i]/sceneScale;corpse.bones.push_back({actor.ragdollDefinition->bodies[i].bone,global,fo3anim::Point(inverse,linear[i],true),fo3anim::Point(inverseRotation,angular[i],true)});}
  if(!fo3player::ValidCorpsePose(corpse))continue;
  actor.corpsePose=std::make_shared<const fo3player::CorpsePose>(std::move(corpse));actor.lastFrame=UINT64_MAX;
  gPlayerSession->player.SetCorpsePose(actor.source.refFormId,*actor.corpsePose);
 }
}

static bool Q230RagdollBounds(const Q230ActorVisual& actor,questrender::Bounds& out){
 if(!actor.corpsePose||!actor.ragdollDefinition||actor.rigs.empty())return false;
 const auto scene=fo3anim::Multiply(actor.dialogueRoot,actor.rigs[0].scenePlacement);
 if(actor.boundsCorpse==actor.corpsePose&&actor.corpseBoundsScene==scene){out=actor.cachedCorpseBounds;return true;}
 out={INFINITY,-INFINITY,INFINITY,-INFINITY,INFINITY,-INFINITY};
 for(size_t i=0;i<actor.corpsePose->bones.size();++i){const auto transform=fo3anim::Multiply(scene,actor.corpsePose->bones[i].global);const auto& body=actor.ragdollDefinition->bodies[i];float scale=0;for(int r=0;r<3;++r)scale+=transform[r]*transform[r];const float margin=body.radius*std::sqrt(scale)+.2f;
  for(const auto& end:{body.a,body.b}){const auto p=fo3anim::Point(transform,end);out.minX=std::min(out.minX,p[0]-margin);out.maxX=std::max(out.maxX,p[0]+margin);out.minY=std::min(out.minY,p[1]-margin);out.maxY=std::max(out.maxY,p[1]+margin);out.minZ=std::min(out.minZ,p[2]-margin);out.maxZ=std::max(out.maxZ,p[2]+margin);}
 }actor.boundsCorpse=actor.corpsePose;actor.corpseBoundsScene=scene;actor.cachedCorpseBounds=out;return true;
}
