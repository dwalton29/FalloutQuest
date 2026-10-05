// Render-thread bridge for physical weapons. Canonical state stays in Player;
// this file owns cached presentation, transient input and physical poses only.
namespace weaponruntime {
using Matrix=fo3anim::Matrix;
using V=fo3vr::V;
using R=fo3vr::R;
struct Model {
  fo3weapon::Asset asset;
  std::vector<GpuObject> objects,flash;
  std::vector<uint8_t> magazines,slides;
  V centre{},half{};
  bool attempted=false,ready=false;
};
struct World {
  Matrix pose=fo3anim::Identity();
  Q223DynamicBody motion;
  uint32_t cell=0,world=0;
  float originX=gSceneCenterXQ1730,originY=gSceneCenterYQ1730,originZ=gSceneFloorZQ1730;
};
std::unordered_map<uint32_t,Model> models;
std::unordered_map<uint64_t,World> world;
fo3weapon::Inputs inputs;
fo3weapon::Trigger trigger;
uint64_t active=0;
bool held=false,support=false,inside=false,magazine=false,charging=false;
Matrix pose=fo3anim::Identity(),magPose=fo3anim::Identity(),magStart=fo3anim::Identity();
V supportLocal{},chargeStart{},magPalm{};
float magQuat[4]{0,0,0,1};
fo3weapon::Zone hip,back,pouch;
Q223DynamicBody motion;
double last=0,recoil=0,ejectUntil=0,flashUntil=0;
float supportWeight=0,chargeTravel=0;
Matrix ejectPose=fo3anim::Identity();
V ejectVelocity{};
uint32_t ejectForm=0;
char ammoText[48]{};
float sceneX=INFINITY,sceneY=INFINITY,sceneZ=INFINITY;

R Rotation(const Matrix&m){return {m[0],m[4],m[8],m[1],m[5],m[9],m[2],m[6],m[10]};}
Matrix Rigid(R r,V p){auto m=fo3anim::Identity();for(int i=0;i<3;i++)for(int j=0;j<3;j++)m[j*4+i]=r[i*3+j];m[12]=p.x;m[13]=p.y;m[14]=p.z;return m;}
V Position(const Matrix&m){return {m[12],m[13],m[14]};}
V Point(const Matrix&m,V v){auto p=fo3anim::Point(m,{v.x,v.y,v.z});return {p[0],p[1],p[2]};}
V Direction(const Matrix&m,V v){auto p=fo3anim::Point(m,{v.x,v.y,v.z},true);return {p[0],p[1],p[2]};}
V GamePoint(std::array<float,3> p){return {p[0]/FO3_UNITS_PER_METRE,p[2]/FO3_UNITS_PER_METRE,-p[1]/FO3_UNITS_PER_METRE};}
Matrix GameRigid(const Matrix&m){
  const R c{1,0,0,0,0,1,0,-1,0};
  return Rigid(fo3vr::Multiply(fo3vr::Multiply(c,Rotation(m)),fo3vr::Transpose(c)),GamePoint({m[12],m[13],m[14]}));
}
void Pulse(int hand,float amplitude=.22f){gWeaponHaptic[hand]=std::max(gWeaponHaptic[hand],amplitude);}
void Log(const char*event,uint64_t id,uint32_t form,uint64_t dynamic=0){
  if(!gPlayerSession)return;
  auto&p=gPlayerSession->player;auto*s=p.Weapon(id);
  if(!s)for(const auto&w:p.Snapshot().worldWeapons)if(w.instance.id==id){s=&w.instance;break;}
  auto i=p.Definitions().items.find(form);if(i==p.Definitions().items.end())return;
  Q6H_LOGI("WEAPON %s: instance=%llu base=%08X loaded=%u reserve=%d condition=%.6f held=%d support=%d holster=%d dynamic=%llu",
    event,(unsigned long long)id,form,s?s->loadedRounds:0,p.AmmoReserve(i->second.weapon.ammo),s?s->condition:0,held,support,!held,(unsigned long long)dynamic);
}
const char* AimGroup(uint32_t animation){
  switch(animation){case 3:case 4:return "1hp";case 5:case 7:return "2hr";case 6:return "2ha";case 8:return "2hh";case 9:return "2hl";default:return nullptr;}
}
Model* Ensure(uint32_t form){
  auto&out=models[form];if(out.attempted)return out.ready?&out:nullptr;out.attempted=true;
  if(!gPlayerSession)return nullptr;
  auto i=gPlayerSession->player.Definitions().items.find(form);
  if(i==gPlayerSession->player.Definitions().items.end()||!i->second.weapon.Firearm())return nullptr;
  const auto&item=i->second;const char*group=AimGroup(item.weapon.animation);
  const auto projectileDefinition=gPlayerSession->player.Definitions().weapons.projectiles.find(item.weapon.projectile);
  if(!group||fo3weapon::AimPath(item.weapon).empty()||projectileDefinition==gPlayerSession->player.Definitions().weapons.projectiles.end()||projectileDefinition->second.type!=1||projectileDefinition->second.explosion||projectileDefinition->second.range<=0)return nullptr;
  std::vector<uint8_t> nif,fp,aim;
  if(!LoadFalloutMeshFile(item.model,nif,nullptr)||
    !LoadFalloutMeshFile("Characters\\_1stPerson\\Skeleton.NIF",fp,nullptr)||
    !LoadFalloutMeshFile(std::string("Characters\\_1stPerson\\")+group+"aim.kf",aim,nullptr)||
    !out.asset.Decode(nif,fp,aim)) {
    Q6H_LOGW("WEAPON ASSET unavailable: base=%08X model=%s authored grip/muzzle required",form,item.model.c_str());return nullptr;
  }
  std::vector<uint8_t> reloadBytes;
  if(item.weapon.reload<26&&LoadFalloutMeshFile(std::string("Characters\\_1stPerson\\")+group+"reload"+char('a'+item.weapon.reload)+".kf",reloadBytes,nullptr))out.asset.Reload(reloadBytes);
  Fo3WorldPlacement placement;placement.modelPath=item.model;placement.baseFormId=form;placement.baseRecordType="WEAP";placement.scale=1;
  std::vector<CpuObject> parts;
  if(!BuildCpuObjects(placement,parts))return nullptr;
  V lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
  for(auto&part:parts){
    const bool mag=out.asset.Descendant(part.mesh.shapeBlock,out.asset.clipNode),slide=out.asset.Descendant(part.mesh.shapeBlock,out.asset.slideNode);
    for(size_t n=0;n<part.mesh.positions.size();n+=3){auto p=GamePoint({part.mesh.positions[n],part.mesh.positions[n+1],part.mesh.positions[n+2]});lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
    GpuObject gpu;if(!UploadCpuObject(part,0,0,0,gpu))continue;
    gpu.physicalWeapon=true;gpu.refFormId=0;out.objects.push_back(std::move(gpu));out.magazines.push_back(mag);out.slides.push_back(slide);
  }
  const auto projectile=gPlayerSession->player.Definitions().weapons.projectiles.find(item.weapon.projectile);
  if(projectile!=gPlayerSession->player.Definitions().weapons.projectiles.end()&&!projectile->second.flashModel.empty()){
    placement.modelPath=projectile->second.flashModel;parts.clear();
    if(BuildCpuObjects(placement,parts))for(auto&part:parts){GpuObject gpu;if(UploadCpuObject(part,0,0,0,gpu)){gpu.physicalWeapon=true;out.flash.push_back(std::move(gpu));}}
  }
  out.centre=(lo+hi)*.5f;out.half=(hi-lo)*.5f;
  for(auto sound:{item.weapon.fire3D,item.weapon.fire2D,item.weapon.dry,item.weapon.equip,item.weapon.unequip})fo3audio::PreloadSound(sound);
  for(const auto&sound:out.asset.reloadSounds)fo3audio::PreloadSound(sound);
  out.ready=!out.objects.empty()&&fo3vr::Finite(out.half);
  Q6H_LOGI("WEAPON ASSET: base=%08X shapes=%zu grip=original-fp-%saim clipNode=%d slideNode=%d muzzle=(%.4f %.4f %.4f)",form,out.objects.size(),group,out.asset.clipNode,out.asset.slideNode,out.asset.muzzle[0],out.asset.muzzle[1],out.asset.muzzle[2]);
  return out.ready?&out:nullptr;
}
Matrix ToScene(const fo3weapon::WorldPose&p){
  fo3anim::Transform t;t.rotation={p.rotation[3],p.rotation[0],p.rotation[1],p.rotation[2]};auto m=fo3anim::ToMatrix(t);
  // Persist absolute FO3 world positions; frame origin changes across CELLs.
  m[12]=(p.position[0]-gSceneCenterXQ1730)/FO3_UNITS_PER_METRE;
  m[13]=FLOOR_Y+(p.position[2]-gSceneFloorZQ1730)/FO3_UNITS_PER_METRE;
  m[14]=SCENE_FORWARD-(p.position[1]-gSceneCenterYQ1730)/FO3_UNITS_PER_METRE;return m;
}
fo3weapon::WorldPose ToSave(const World&w){
  fo3weapon::WorldPose p;p.cell=w.cell;p.world=w.world;
  p.position={w.originX+w.pose[12]*FO3_UNITS_PER_METRE,w.originY-(w.pose[14]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,w.originZ+(w.pose[13]-FLOOR_Y)*FO3_UNITS_PER_METRE};
  auto q=fo3anim::QuaternionFromMatrix(Rotation(w.pose).data());p.rotation={q[1],q[2],q[3],q[0]};
  const auto&v=w.motion.linearVelocity;const auto&a=w.motion.angularVelocity;p.velocity={v.x,v.y,v.z};p.angularVelocity={a.x,a.y,a.z};return p;
}
void Advance(World&w,const Model&model,float dt){
  // Same authored collision box query and SI gravity/substep policy as loose
  // Q223 objects; runtime ID is never passed as a fabricated Bethesda REFR.
  auto&b=w.motion;if(b.sleeping)return;
  while(dt>1e-5f){float step=std::min(dt,.016f);dt-=step;
    const V centre=Point(w.pose,model.centre);b.linearVelocity.y-=9.80665f*step;
    const V next=centre+VrPoint(b.linearVelocity)*step;
    V resolved=next,normal{};uint32_t contacts=0,candidates=0;
    auto r=Rotation(w.pose);auto x=fo3vr::Rotate(r,{1,0,0}),y=fo3vr::Rotate(r,{0,1,0}),z=fo3vr::Rotate(r,{0,0,1});
    ResolveFo3DynamicBoxQ225(0,centre.x,centre.y,centre.z,next.x,next.y,next.z,
      model.half.x,model.half.y,model.half.z,x.x,x.y,x.z,y.x,y.y,y.z,z.x,z.y,z.z,fo3vr::Length(model.half),
      &resolved.x,&resolved.y,&resolved.z,&normal.x,&normal.y,&normal.z,&contacts,&candidates);
    if(contacts){normal=fo3vr::Unit(normal,{0,1,0});auto velocity=VrPoint(b.linearVelocity);float inward=fo3vr::Dot(velocity,normal);if(inward<0)velocity=velocity-normal*inward;
      b.linearVelocity=RuntimePoint(velocity*std::exp(-1.25f*step));b.angularVelocity=Q211Mul(b.angularVelocity,std::exp(-2.f*step));
      if(normal.y>.45f&&Q211Length(b.linearVelocity)<.08f&&Q211Length(b.angularVelocity)<.22f)b.sleepTimer+=step;else b.sleepTimer=0;
      if(b.sleepTimer>.3f){b.sleeping=true;b.linearVelocity={};b.angularVelocity={};}
    }
    Matrix delta;Q223BuildMotionDelta(RuntimePoint(centre),RuntimePoint(resolved),b.angularVelocity,step,delta.data());w.pose=fo3anim::Multiply(delta,w.pose);
  }
}
bool Resident(const fo3weapon::WorldPose&p){
  if(!gExteriorWorldspaceQ1890)return p.world==0&&p.cell==gCurrentCellFormId;
  if(p.world!=gExteriorWorldspaceQ1890)return false;
  return p.world!=0x3c||fo3weapon::CollisionResident(p,gExteriorWindowGridXQ1890,gExteriorWindowGridYQ1890);
}
Matrix HandPose(const Model&m){
  const int hand=fo3anim::FindBone(gVrSkeleton,"Bip01 R Hand");
  if(hand<0)return pose;
  const auto bind=GameRigid(gVrSkeleton.bindGlobal[hand]);
  const auto&solved=gVrSolvedArms[1];
  Matrix root;std::copy(gQ210PlayerRoot,gQ210PlayerRoot+16,root.begin());
  auto wrist=solved.hand.Point({bind[12]+0,bind[13]+FLOOR_Y,bind[14]+SCENE_FORWARD});
  const auto local=Rigid(fo3vr::Multiply(solved.hand.rotation,Rotation(bind)),wrist);
  return fo3anim::Multiply(fo3anim::Multiply(root,local),GameRigid(m.asset.handToWeapon));
}
void Zones(){
  Matrix root;std::copy(gQ210PlayerRoot,gQ210PlayerRoot+16,root.begin());auto body=Rotation(root);
  const int thigh=fo3anim::FindBone(gVrSkeleton,"Bip01 R Thigh"),pelvis=fo3anim::FindBone(gVrSkeleton,"Bip01 Pelvis");
  if(thigh<0||pelvis<0)return;
  hip=fo3weapon::Hip(VrPoint(Q211TransformPoint(gQ210PlayerRoot,VrSkeletonPoint(thigh))),body);
  back=fo3weapon::Back(VrPoint(Q211TransformPoint(gQ210PlayerRoot,RuntimePoint(gVrSolvedArms[1].shoulder))),body);
  pouch=fo3weapon::Pouch(VrPoint(Q211TransformPoint(gQ210PlayerRoot,VrSkeletonPoint(pelvis))),body);
}
struct Flight {bool active=false;uint64_t instance=0;uint32_t projectile=0,base=0;V position{},velocity{};float distance=0,limit=0,damage=0;};
std::array<Flight,64> flights{};
uint32_t randomState=0x463351;
float Random(){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return (randomState&0xffffff)/float(0x1000000);}
bool Hit(uint64_t instance,V origin,V direction,float limit,uint32_t base,float damage){
  if(!gPlayerSession)return false;
  auto&p=gPlayerSession->player;uint32_t target=0;float nearest=limit;
  for(const auto&actor:gQ230NpcActors){
    if(p.ActorHealth(actor.source.refFormId)<=0)continue;
    // Visibility envelopes are broad phase only; actual skinned authored
    // surfaces determine whether a bullet hits the actor.
    const auto&bounds=actor.renderBounds;float entry=0;
    if(!actor.renderBoundsReady||!fo3interaction::Box({origin.x,origin.y,origin.z},{direction.x,direction.y,direction.z},
      {bounds.minX,bounds.minY,bounds.minZ},{bounds.maxX,bounds.maxY,bounds.maxZ},nearest,entry))continue;
    const fo3interaction::Point o{origin.x,origin.y,origin.z},d{direction.x,direction.y,direction.z};
    for(const auto&rig:actor.rigs){
      for(size_t v=0;v+2<rig.hitVertices.size();v+=3){
        std::array<fo3interaction::Point,3> triangle;
        for(size_t corner=0;corner<3;corner++){
          const auto blended=fqskin::Blend(rig.skin.mapping.attributes.data()+(v+corner)*8,rig.skin.palette,false);
          triangle[corner]=fqskin::Transform(blended.data(),rig.hitVertices[v+corner].data(),false);
        }
        if(fo3weapon::Surface(o,d,triangle[0],triangle[1],triangle[2],nearest))target=actor.source.refFormId;
      }
    }
  }
  if(target&&!HasFo3InteractionOccluder(origin.x,origin.y,origin.z,direction.x,direction.y,direction.z,nearest,target)){
    if(p.ApplyWeaponHit(base,target,damage)){if(Fo3DialogueFocus())EndFo3Dialogue("weapon damage");Q6H_LOGI("WEAPON HIT: instance=%llu target=%08X health=%.3f",(unsigned long long)instance,target,p.ActorHealth(target));}
    return true;
  }
  return HasFo3InteractionOccluder(origin.x,origin.y,origin.z,direction.x,direction.y,direction.z,limit,0);
}
void Shoot(uint64_t instance,const fo3weapon::Definition&d,const Model&model,float shotDamage){
  const auto&defs=gPlayerSession->player.Definitions().weapons;const auto q=defs.projectiles.find(d.projectile);if(q==defs.projectiles.end())return;
  const auto&projectile=q->second;const auto muzzle=Point(pose,GamePoint(model.asset.muzzle));
  // Models are authored with their barrel along local X. Named muzzle anchor
  // supplies origin; the original minimum spread supplies the angular cone.
  auto forward=fo3vr::Unit(Direction(pose,GamePoint(model.asset.muzzleForward)));auto right=fo3vr::Unit(Direction(pose,{0,0,1})),up=fo3vr::Unit(Direction(pose,{0,0,-1}));
  for(unsigned pellet=0;pellet<d.pellets;++pellet){
    const float angle=Random()*2*fo3vr::Pi,spread=std::tan(d.minSpread*fo3vr::Pi/180)*std::sqrt(Random());
    const auto direction=fo3vr::Unit(forward+right*(std::cos(angle)*spread)+up*(std::sin(angle)*spread));
    const float limit=projectile.range/FO3_UNITS_PER_METRE;
    const auto*owned=gPlayerSession->player.Weapon(instance);if(!owned)return;
    const uint32_t base=owned->formId;const float damage=shotDamage/d.pellets;
    if(projectile.Hitscan())Hit(instance,muzzle,direction,limit,base,damage);
    else for(auto&f:flights)if(!f.active){f={true,instance,d.projectile,base,muzzle,direction*(projectile.speed/FO3_UNITS_PER_METRE),0,limit,damage};break;}
  }
}
void AdvanceFlights(float dt){
  for(auto&f:flights){if(!f.active)continue;
    const auto q=gPlayerSession->player.Definitions().weapons.projectiles.find(f.projectile);
    if(q==gPlayerSession->player.Definitions().weapons.projectiles.end()){f.active=false;continue;}
    float remaining=dt;
    while(remaining>1e-5f&&f.active){const float step=std::min(remaining,.016f);remaining-=step;
      f.velocity.y-=q->second.gravity*9.80665f*step;const auto move=f.velocity*step;const float distance=fo3vr::Length(move);
      const float travel=std::min(distance,f.limit-f.distance);
      if(travel<=0||Hit(f.instance,f.position,fo3vr::Unit(move),travel,f.base,f.damage)){f.active=false;break;}
      f.position=f.position+move;f.distance+=distance;if(f.distance>=f.limit)f.active=false;
    }
  }
}
void RetireReload(){magazine=charging=support=false;supportWeight=chargeTravel=0;}
void Draw(uint64_t id,uint32_t form){active=id;held=true;inside=false;RetireReload();motion={};Pulse(1);fo3audio::SoundEvent(gPlayerSession->player.Definitions().items.at(form).weapon.equip);Log("DRAW",id,form);}
void DrawModel(Model&m,const Matrix&root,bool alpha,bool onlyMag=false,bool removeMag=false,float action=0){
  auto transform=root;
  // Uploaded vertices include the runtime's fixed model-space origin.
  auto offset=fo3anim::Identity();offset[13]=-FLOOR_Y;offset[14]=-SCENE_FORWARD;transform=fo3anim::Multiply(root,offset);
  for(size_t n=0;n<m.objects.size();n++){if((onlyMag&&!m.magazines[n])||(removeMag&&m.magazines[n]))continue;
    auto&o=m.objects[n];if(o.alphaBlend!=alpha)continue;
    auto partTransform=transform;
    if(action>0&&m.slides[n]){auto travel=fo3anim::Identity();travel[12]=-action;partTransform=fo3anim::Multiply(transform,travel);}
    std::copy(partTransform.begin(),partTransform.end(),o.q220DynamicTransform);
    PrepareFo3InteriorObjectLights(o,nullptr);
    glDepthMask(o.zBufferWriteQ1200?GL_TRUE:GL_FALSE);glEnable(GL_DEPTH_TEST);
    if(alpha){glEnable(GL_BLEND);glBlendFunc(Q1150BlendFactor(o.alphaSourceBlend,true),Q1150BlendFactor(o.alphaDestBlend,false));}
    DrawSceneObject(o);
  }
}
void DrawFlash(Model&m,bool alpha){
  auto transform=fo3anim::Multiply(pose,GameRigid(m.asset.nodes.bindGlobal[fo3anim::FindBone(m.asset.nodes,"ProjectileNode")]));
  auto offset=fo3anim::Identity();offset[13]=-FLOOR_Y;offset[14]=-SCENE_FORWARD;transform=fo3anim::Multiply(transform,offset);
  for(auto&o:m.flash){if(o.alphaBlend!=alpha)continue;std::copy(transform.begin(),transform.end(),o.q220DynamicTransform);PrepareFo3InteriorObjectLights(o,nullptr);glDepthMask(o.zBufferWriteQ1200?GL_TRUE:GL_FALSE);glEnable(GL_DEPTH_TEST);if(alpha){glEnable(GL_BLEND);glBlendFunc(Q1150BlendFactor(o.alphaSourceBlend,true),Q1150BlendFactor(o.alphaDestBlend,false));}DrawSceneObject(o);}
}
void AlignSupportHand(const Model&m){
  // The two-point solve already aligns the physical palm. Transport the visible
  // hand/finger orientation from the original FP aim, ignoring left-controller roll.
  const int bone=fo3anim::FindBone(gVrSkeleton,"Bip01 L Hand");if(bone<0)return;
  Matrix root;std::copy(gQ210PlayerRoot,gQ210PlayerRoot+16,root.begin());Matrix inverse;if(!fo3anim::Inverse(root,inverse))return;
  const auto current=fo3vr::Multiply(gVrSolvedArms[0].hand.rotation,Rotation(GameRigid(gVrSkeleton.bindGlobal[bone])));
  const auto desired=Rotation(fo3anim::Multiply(inverse,fo3anim::Multiply(pose,GameRigid(m.asset.weaponToLeft))));
  const auto change=fo3vr::Multiply(desired,fo3vr::Transpose(current));const V palm=Point(inverse,VrPoint(gWeaponPalm[0]));
  const auto delta=Rigid(change,palm-fo3vr::Rotate(change,palm)),direction=Rigid(change,{});
  for(auto&part:gQ211PlayerRigParts){bool changed=false;
    for(size_t slot=0;slot<part.skin.mapping.sources.size();slot++){
      const int source=part.skin.mapping.sources[slot];if(part.roles[source]!=3)continue;
      auto&row=part.skin.palette[slot+1];Matrix p,d;std::copy(row.begin(),row.begin()+16,p.begin());std::copy(row.begin()+16,row.end(),d.begin());
      fqskin::Pack(row,fo3anim::Multiply(delta,p),fo3anim::Multiply(direction,d));changed=true;
    }
    if(changed&&part.gpuIndex<gQ210PlayerBody.size())QActorUploadSkin(gQ210PlayerBody[part.gpuIndex],part.skin,fqactor::player,true);
  }
}
}

bool WeaponHeld(){return weaponruntime::held;}
void WeaponPersistWorld(){
  using namespace weaponruntime;if(!gPlayerSession)return;
  for(const auto&e:world)gPlayerSession->player.UpdateWorldWeapon(e.first,ToSave(e.second));
}
void WeaponShutdown(){
  using namespace weaponruntime;WeaponPersistWorld();for(auto&e:models){Q74DeleteGpuObjects(e.second.objects);Q74DeleteGpuObjects(e.second.flash);}models.clear();world.clear();
  active=0;held=false;RetireReload();inputs={};flights={};last=0;ammoText[0]=0;fo3ammoui::Shutdown();
}
void WeaponRender(bool alpha){
  using namespace weaponruntime;if(!gPlayerSession||IsFo3LoadingVisible())return;
  const auto&p=gPlayerSession->player;const auto*s=p.EquippedWeapon();
  if(s&&s->id==active){auto m=models.find(s->formId);if(m!=models.end()&&m->second.ready){DrawModel(m->second,pose,alpha,false,s->needsAction&&s->loadedRounds==0,charging?chargeTravel:0);if(magazine)DrawModel(m->second,magPose,alpha,true);if(held&&flashUntil>last)DrawFlash(m->second,alpha);}}
  for(const auto&w:p.Snapshot().worldWeapons){auto v=world.find(w.instance.id);auto m=models.find(w.instance.formId);if(v!=world.end()&&m!=models.end()&&m->second.ready)DrawModel(m->second,v->second.pose,alpha,false,w.instance.needsAction&&w.instance.loadedRounds==0);}
  if(ejectUntil>last){auto m=models.find(ejectForm);if(m!=models.end()&&m->second.ready)DrawModel(m->second,ejectPose,alpha,true);}
}
void WeaponUpdate(double now,bool backButton){
  using namespace weaponruntime;
  const float dt=std::clamp(float(now-last),0.f,.064f);last=now;
  const bool blocked=Fo3PipboyFocus()||IsFo3LoadingVisible()||!gSceneReady||!gShoulderFocused||!gPlayerSession||gPlayerSession->saveBlocked;
  inputs.Update(blocked,gQ217FingerGrip[1],gQ217FingerGrip[0],gQ217FingerTrigger[1],backButton);
  if(Fo3DialogueFocus()){inputs.r=false;inputs.l=false;} // Dialogue suppresses new grabs; held trigger can interrupt.
  if(!gPlayerSession)return;auto&p=gPlayerSession->player;
  // Explicit milestone bootstrap, never part of authored starting inventory.
  if(!gPlayerSession->saveBlocked&&!p.Snapshot().developmentWeaponGranted){if(p.BootstrapDevelopmentWeapon()){auto*s=p.EquippedWeapon();if(s)Log("GRANT",s->id,s->formId);}}
  auto*s=p.EquippedWeapon();
  if(!s||s->id!=active){active=s?s->id:0;held=false;inside=false;RetireReload();trigger={};motion={};if(s)Log("EQUIP",s->id,s->formId);}
  if(sceneX!=gSceneCenterXQ1730||sceneY!=gSceneCenterYQ1730||sceneZ!=gSceneFloorZQ1730){
    WeaponPersistWorld();world.clear();flights={};
    sceneX=gSceneCenterXQ1730;sceneY=gSceneCenterYQ1730;sceneZ=gSceneFloorZQ1730;
  }
  // Cache owned and saved-world resources independently of trigger events.
  for(const auto&i:p.Snapshot().inventory){const auto definition=p.Definitions().items.find(i.formId);if(definition!=p.Definitions().items.end()&&definition->second.weapon.Firearm())Ensure(i.formId);}
  for(const auto&w:p.Snapshot().worldWeapons)if(Resident(w.pose)){
    auto*m=Ensure(w.instance.formId);if(!m)continue;
    auto found=world.find(w.instance.id);if(found==world.end()){
      World v;v.pose=ToScene(w.pose);v.cell=w.pose.cell;v.world=w.pose.world;v.motion.linearVelocity={w.pose.velocity[0],w.pose.velocity[1],w.pose.velocity[2]};v.motion.angularVelocity={w.pose.angularVelocity[0],w.pose.angularVelocity[1],w.pose.angularVelocity[2]};world.emplace(w.instance.id,v);
    }else if(!Resident(ToSave(found->second)))p.UpdateWorldWeapon(w.instance.id,ToSave(found->second));
    else if(!blocked)Advance(found->second,*m,dt);
  }
  // A CELL origin change is handled by persisted world positions at transition.
  for(auto it=world.begin();it!=world.end();){bool keep=false;for(const auto&w:p.Snapshot().worldWeapons)if(w.instance.id==it->first&&Resident(w.pose)){keep=true;break;}if(!keep){p.UpdateWorldWeapon(it->first,ToSave(it->second));it=world.erase(it);}else ++it;}
  if(blocked){
    if(held){held=false;if(s)Log("HOLSTER FOCUS",s->id,s->formId);}
    RetireReload();inside=false;
    if(s&&gQ210PlayerBodyReady){Zones();const auto&zone=p.Definitions().items.at(s->formId).weapon.LongGun()?back:hip;const bool longGun=p.Definitions().items.at(s->formId).weapon.LongGun();const V display=longGun?zone.center:fo3weapon::HipDisplay(zone);pose=Rigid(fo3vr::Multiply(zone.body,fo3vr::Axis({0,0,1},longGun?fo3vr::Pi*.5f:-fo3vr::Pi*.5f)),display);}
    for(int hand=0;hand<2;hand++)gQ220Grab[hand].previousGrip=gQ217FingerGrip[hand];return;
  }
  Zones();
  AdvanceFlights(dt);
  Model*m=s?Ensure(s->formId):nullptr;
  if(s&&m){const auto&d=p.Definitions().items.at(s->formId).weapon;auto&zone=d.LongGun()?back:hip;
    if(!held){const V display=d.LongGun()?zone.center:fo3weapon::HipDisplay(zone);pose=Rigid(fo3vr::Multiply(zone.body,fo3vr::Axis({0,0,1},d.LongGun()?fo3vr::Pi*.5f:-fo3vr::Pi*.5f)),display);}
    else if(gWeaponPalmValid[1]){
      pose=HandPose(*m);
      const V right=VrPoint(gWeaponPalm[1]),left=VrPoint(gWeaponPalm[0]);
      const V supportAnchor=GamePoint(m->asset.support);
      // Authored support palm with a 20cm sliding fore-end access segment.
      const V begin=Point(pose,supportAnchor+V{-.10f,0,0}),end=Point(pose,supportAnchor+V{.10f,0,0});
      if(support&&(!gWeaponPalmValid[0]||gQ217FingerGrip[0]<=.25f)){support=false;supportWeight=0;Log("SUPPORT RELEASE",s->id,s->formId);}
      if(inputs.l&&!gQ220Grab[0].active&&gWeaponPalmValid[0]&&d.LongGun()&&m->asset.hasSupport&&!magazine&&!charging){
        const auto point=fo3weapon::ClosestSupport(left,begin,end);
        if(fo3vr::Length(left-point)<.12f){Matrix inverse;if(fo3anim::Inverse(pose,inverse)){supportLocal=Point(inverse,point);support=true;Pulse(0);Log("SUPPORT",s->id,s->formId);}}
      }
      if(support&&gWeaponPalmValid[0]){
        const auto gripLocal=Point(GameRigid(m->asset.handToWeapon),{});
        // Keep the actual primary palm invariant under the analytic swing.
        Matrix inverse;if(fo3anim::Inverse(pose,inverse)){
          supportWeight=std::min(1.f,supportWeight+dt*12);
          const auto primaryLocal=Point(inverse,right);const auto r=fo3weapon::TwoHand(Rotation(pose),right,left,supportLocal-primaryLocal,supportWeight);
          pose=Rigid(r,right-fo3vr::Rotate(r,primaryLocal));
        }
        (void)gripLocal;
      }
      if(support&&gWeaponPalmValid[0])AlignSupportHand(*m);
      const auto sampledRotation=fo3anim::QuaternionFromMatrix(Rotation(pose).data());
      const float sampledQuat[4]{sampledRotation[1],sampledRotation[2],sampledRotation[3],sampledRotation[0]};
      Q223SampleHeldMotion(motion,RuntimePoint(Point(pose,m->centre)),sampledQuat);
      const bool next=zone.Contains(right,inside);if(next&&!inside)Pulse(1,.12f);inside=next;
      if(gQ217FingerGrip[1]<=.25f){
        if(inside){held=false;RetireReload();Pulse(1);fo3audio::SoundEvent(d.unequip);Log("HOLSTER",s->id,s->formId);}
        else{World v;v.pose=pose;v.cell=gCurrentCellFormId;v.world=gExteriorWorldspaceQ1890;v.motion.linearVelocity=motion.sampledLinearVelocity;v.motion.angularVelocity=motion.sampledAngularVelocity;
          const auto id=s->id;const auto form=s->formId;
          if(p.DropWeapon(id,ToSave(v))){world[id]=v;held=false;RetireReload();active=0;Log("DROP",id,form,id);s=nullptr;}
          else{held=false;RetireReload();Log("HOLSTER DROP DENIED",id,form);}
        }
      }
      const auto reloadFamily=m->asset.ReloadInteraction(d);
      if(s&&held&&(reloadFamily==fo3weapon::ReloadFamily::DetachablePistol||reloadFamily==fo3weapon::ReloadFamily::DetachableRifle)){
        const auto id=s->id;const auto form=s->formId;
        if(inputs.eject&&!support){
          if(p.EjectMagazine(id)){magazine=charging=false;ejectPose=pose;ejectVelocity={0,-.3f,0};ejectUntil=now+3;ejectForm=form;fo3audio::NamedSound(m->asset.reloadSounds[0]);Log("MAG OUT",id,form);s=p.Weapon(id);}
        }
        if(inputs.l&&!gQ220Grab[0].active&&!support&&gWeaponPalmValid[0]){
          if(s->needsAction&&!s->loadedRounds&&!magazine&&p.AmmoReserve(d.ammo)>0&&pouch.Contains(left)){
            magazine=true;magStart=Rigid(Rotation(pose),left-fo3vr::Rotate(Rotation(pose),GamePoint(m->asset.clip)));magPalm=left;std::copy(gQ218LeftHandQuat,gQ218LeftHandQuat+4,magQuat);Pulse(0);Log("MAG POUCH",id,form);
          }else if(s->needsAction&&s->loadedRounds&&fo3vr::Length(left-Point(pose,GamePoint(m->asset.slide)))<.10f){Matrix inverse;if(fo3anim::Inverse(pose,inverse)){charging=true;chargeTravel=0;chargeStart=Point(inverse,left);Pulse(0,.12f);}}
        }
        if(magazine&&!gWeaponPalmValid[0])magazine=false;
        if(magazine){Matrix delta;Q220ControllerDeltaMatrix(RuntimePoint(magPalm),magQuat,gWeaponPalm[0],gQ218LeftHandQuat,delta.data());magPose=fo3anim::Multiply(delta,magStart);
          const auto distance=fo3vr::Length(Point(magPose,GamePoint(m->asset.clip))-Point(pose,GamePoint(m->asset.clip)));
          const auto alignment=fo3vr::Dot(Direction(magPose,{0,0,-1}),Direction(pose,{0,0,-1}));
          if(gQ217FingerGrip[0]<=.25f){magazine=false;if(distance<.085f&&alignment>.70f&&p.LoadMagazine(id)){Pulse(0);fo3audio::NamedSound(m->asset.reloadSounds[1]);Log("RELOAD INSERT",id,form);s=p.Weapon(id);}}
        }
        if(charging&&!gWeaponPalmValid[0])charging=false;
        if(charging){chargeTravel=fo3weapon::RearwardPull(Rotation(pose),Position(pose),left,chargeStart);if(chargeTravel>=.045f&&p.ChamberWeapon(id)){charging=false;Pulse(0);fo3audio::NamedSound(m->asset.reloadSounds[2]);Log("RELOAD CHAMBER",id,form);s=p.Weapon(id);}if(gQ217FingerGrip[0]<=.25f)charging=false;}
      }
      if(s&&held&&gWeaponPalmValid[1]&&trigger.Ready(d,inputs,gQ217FingerTrigger[1],now)){
        if(Fo3DialogueFocus())EndFo3Dialogue("player weapon fire");
        const auto id=s->id;const auto form=s->formId;
        const float shotDamage=p.WeaponDamage(id);
        if(p.FireWeapon(id)){const auto projectile=p.Definitions().weapons.projectiles.find(d.projectile);if(projectile!=p.Definitions().weapons.projectiles.end())flashUntil=now+projectile->second.flashDuration;Shoot(id,d,*m,shotDamage);fo3audio::SoundEvent(d.fire3D?d.fire3D:d.fire2D);Pulse(1,std::clamp(d.rumbleRight,.15f,.65f));recoil=1;Log("FIRE",id,form);}
        else{fo3audio::SoundEvent(d.dry);Pulse(1,.06f);Log("EMPTY",id,form);}
        s=p.Weapon(id);
      }
      // Comfortable short VR recoil, controller remains the primary anchor.
      recoil*=std::exp(-20*dt);
      if(recoil>1e-4){const float weight=support?.45f:1.f;auto kick=fo3vr::Axis({0,0,1},float(recoil)*.035f*weight);pose=Rigid(fo3vr::Multiply(Rotation(pose),kick),Position(pose)+Direction(pose,{-float(recoil)*.006f*weight,0,0}));}
    }
  }
  // Right primary-grip priority: body weapon, dynamic exact instance, authored
  // weapon REFR, then existing non-weapon loose-object grabbing.
  if(inputs.r&&gWeaponPalmValid[1]&&!held&&!gQ220Grab[1].active){
    const auto hand=VrPoint(gWeaponPalm[1]);
    if(s&&m&&(p.Definitions().items.at(s->formId).weapon.LongGun()?back:hip).Contains(hand))Draw(s->id,s->formId);
    else{
      uint64_t best=0;float distance=.16f;
      for(const auto&w:p.Snapshot().worldWeapons){auto v=world.find(w.instance.id);if(v==world.end())continue;auto model=models.find(w.instance.formId);if(model==models.end())continue;float d=fo3weapon::BoxDistance(Rotation(v->second.pose),Position(v->second.pose),hand,model->second.centre,model->second.half);if(d<distance){best=w.instance.id;distance=d;}}
      if(best&&p.PickupWorldWeapon(best)){world.erase(best);s=p.EquippedWeapon();Draw(s->id,s->formId);Log("PICKUP",s->id,s->formId,best);}
      else {uint32_t ref=0;Vec3 centre;float surface=0;if(Q220FindNearestLooseRef(gWeaponPalm[1],ref,centre,surface)){
        const auto r=p.Definitions().references.find(ref);
        if(r!=p.Definitions().references.end()){auto item=p.Definitions().items.find(r->second.base);
          if(item!=p.Definitions().items.end()&&item->second.weapon.Firearm()&&Ensure(item->first)&&p.PickupWeapon(ref)){
            gQ223DynamicBodies.erase(ref);SetFo3CollectedCollisionRefs(p.Snapshot().collected);s=p.EquippedWeapon();Draw(s->id,s->formId);Log("PICKUP",s->id,s->formId);
          }
        }
      }}
    }
  }
  if(ejectUntil>now){ejectVelocity.y-=9.80665f*dt;ejectPose[12]+=ejectVelocity.x*dt;ejectPose[13]+=ejectVelocity.y*dt;ejectPose[14]+=ejectVelocity.z*dt;}
  Q223AdvanceDynamicBodies();
  if(!support&&!magazine&&!charging)Q220UpdateLooseGrab(0,gWeaponPalmValid[0],gWeaponPalm[0],gQ218LeftHandQuat,gQ217FingerGrip[0]);
  if(!held)Q220UpdateLooseGrab(1,gWeaponPalmValid[1],gWeaponPalm[1],gQ218RightHandQuat,gQ217FingerGrip[1]);
  else gQ220Grab[1].previousGrip=gQ217FingerGrip[1];
  s=p.EquippedWeapon();ammoText[0]=0;if(s&&held){const auto&d=p.Definitions().items.at(s->formId).weapon;std::snprintf(ammoText,sizeof(ammoText),"%u / %d",s->loadedRounds,p.AmmoReserve(d.ammo));}
  fo3ammoui::Prepare(ammoText);
}
