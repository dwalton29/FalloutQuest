// Logical-frame V.A.T.S. bridge. Each eye only draws the prepared documents.
namespace vatsruntime {
using fo3vr::V;
using fo3anim::Matrix;
using fo3menuui::Document;
using fo3weapon::BodyPart;
struct Limb {
  const BodyPart* part=nullptr;
  V point{};
  unsigned chance=0;
  float condition=1;
};
struct View {
  uint32_t target=0;
  std::string name;
  float health=0;
  std::vector<Limb> limbs;
  uint8_t selected=fo3vats::NoPart;
};
fo3vats::Input input;
fo3vats::Session session;
View view;
Document chrome;
std::array<Document,14> limbDocs;
std::array<fo3menu::Box,14> limbLabelBounds;
uint32_t cell=0,worldspace=0;
uint64_t generation=0;
const fo3player::Player* owner=nullptr;
double nextRound=0,flashEnd=0;
std::string status;
double statusUntil=0;
bool savedShot=false;

Q230ActorVisual* Actor(uint32_t id){
  const auto found=std::find_if(gQ230NpcActors.begin(),gQ230NpcActors.end(),[&](const auto& actor){
    return actor.source.refFormId==id&&!actor.runtime.offScene&&actor.runtime.Alive();
  });
  return found==gQ230NpcActors.end()?nullptr:&*found;
}
bool Anchor(Q230ActorVisual& actor,const BodyPart& part,V& point){
  std::array<float,3> bone{};
  if(!Q230LiveBone(actor,fo3anim::FindBone(actor.skeleton,part.target),bone)&&
     !Q230LiveBone(actor,fo3anim::FindBone(actor.skeleton,part.node),bone))return false;
  point={bone[0],bone[1],bone[2]};
  return fo3vr::Finite(point);
}
bool Clear(V origin,V point,uint32_t target){
  const V delta=point-origin;
  const float distance=fo3vr::Length(delta);
  if(!std::isfinite(distance)||distance<=.02f)return false;
  const auto ray=delta*(1.f/distance);
  Fo3WorldSurfaceHit surface;
  return !TraceFo3WorldSurface(origin.x,origin.y,origin.z,ray.x,ray.y,ray.z,distance,
                               fo3player::PlayerRef,target,&surface)||surface.distance>=distance-.015f;
}
unsigned Chance(const fo3player::Player& p,const fo3weapon::Definition& weapon,
                const BodyPart& part,V origin,V point,uint32_t target){
  const auto& data=p.Definitions().weapons;
  float distance=fo3vr::Length(point-origin)*FO3_UNITS_PER_METRE;
  const auto projectile=data.projectiles.find(weapon.projectile);
  if(projectile==data.projectiles.end()||distance>projectile->second.range)return 0;
  const float skill=weapon.skill>=32&&weapon.skill<=45?p.EffectiveSkill(weapon.skill-32):0;
  return fo3vats::Chance(data.vats,weapon,part,distance,skill,Clear(origin,point,target));
}

View Target(const fo3player::Player& player,const fo3weapon::Definition& weapon,V origin,V direction){
  View result;
  const auto& settings=player.Definitions().weapons.vats;
  const float maxDistance=settings.maxDistance/FO3_UNITS_PER_METRE;
  const float threshold=std::cos(float(settings.targetAngle)*fo3vr::Pi/180);
  float best=threshold,bestDistance=INFINITY;
  for(auto& actor:gQ230NpcActors){
    if(actor.runtime.offScene||!actor.runtime.Alive())continue;
    const uint32_t reference=actor.source.refFormId;
    if(player.ActorHealth(reference)<=0)continue;
    const auto* parts=player.ActorBodyParts(reference);
    if(!parts||actor.rigs.empty())continue;
    const V root{actor.runtime.position[0],actor.runtime.position[1],actor.runtime.position[2]};
    if(fo3vr::Length(root-origin)>maxDistance)continue;
    for(const auto& part:*parts){
      if(part.type>=14)continue;
      V point;
      if(!Anchor(actor,part,point))continue;
      const float score=fo3vats::AimScore(origin,direction,point,threshold);
      const float distance=fo3vr::Length(point-origin);
      if(score<best||(score==best&&distance>=bestDistance)||!Clear(origin,point,reference))continue;
      best=score;bestDistance=distance;
      result.target=reference;
      result.selected=part.type;
    }
  }
  if(auto* actor=Actor(result.target)){
    result.name=actor->source.fullName;
    result.health=player.HealthPercentage(result.target);
    if(const auto* parts=player.ActorBodyParts(result.target))for(const auto& part:*parts){
      if(part.type>=14)continue;
      V point;
      if(Anchor(*actor,part,point)){
        result.limbs.push_back({&part,point,Chance(player,weapon,part,origin,point,result.target),
                                player.ActorLimbCondition(result.target,part.type)});
      }
    }
  }
  return result;
}

Matrix AimPose(const weaponruntime::Model& model,V point){
  using namespace weaponruntime;
  const V local=GamePoint(model.asset.muzzle);
  const V muzzle=Point(pose,local);
  const V forward=fo3vr::Unit(Direction(pose,GamePoint(model.asset.muzzleForward)));
  const V aimed=fo3vr::Unit(point-muzzle,forward);
  const auto roll=Direction(pose,{0,1,0});
  const auto rotation=fo3vr::Multiply(fo3vr::FrameRotation(forward,roll,aimed,roll),Rotation(pose));
  return Rigid(rotation,muzzle-fo3vr::Rotate(rotation,local));
}

void Finish(bool cue){
  if(!session.Active())return;
  session={};view={};owner=nullptr;status.clear();statusUntil=0;
  weaponruntime::flashUntil=0;
  if(savedShot){FlushFo3PlayerState();savedShot=false;}
  if(cue)fo3audio::NamedSound("UIVATSExit");
}

void Queue(const fo3player::Player& player,const fo3player::Stack& stack,
           const fo3weapon::Definition& weapon,double now){
  const auto limb=std::find_if(view.limbs.begin(),view.limbs.end(),[](const auto& item){
    return item.part->type==view.selected;
  });
  if(limb==view.limbs.end())return;
  const auto& settings=player.Definitions().weapons.vats;
  const auto result=session.Queue({view.target,view.selected,fo3vats::Burst(settings,weapon),
      limb->chance,fo3vats::Cost(settings,weapon)},player.ActionPoints(),stack.loadedRounds/weapon.ammoUse);
  if(result==fo3vats::QueueResult::Added){
    status.clear();fo3audio::NamedSound("UIVATSSelectTargetPart");
    return;
  }
  const char* key=result==fo3vats::QueueResult::NoAmmo?"sVATSMessageNoAmmo":
                  result==fo3vats::QueueResult::NoChance?"sVATSMessageZeroChance":"sVATSMessageLowAP";
  status=fo3menuui::Setting(player,key);
  statusUntil=now+1.6;
  fo3audio::NamedSound("UIVATSInsufficientAP");
}

void Execute(fo3player::Player& player,const fo3player::Stack& stack,
             const fo3weapon::Definition& weapon,const weaponruntime::Model& model,double raw,double game){
  const auto* shot=session.Current();
  if(!shot){if(raw>=nextRound)Finish(true);return;}
  if(raw<nextRound)return;
  auto* actor=Actor(shot->target);
  const auto* parts=player.ActorBodyParts(shot->target);
  if(!actor||!parts||player.ActorHealth(shot->target)<=0){session.Skip();return;}
  const auto part=std::find_if(parts->begin(),parts->end(),[&](const auto& p){return p.type==shot->part;});
  V point;
  if(part==parts->end()||!Anchor(*actor,*part,point)){session.Skip();return;}
  using namespace weaponruntime;
  const V muzzle=Point(pose,GamePoint(model.asset.muzzle));
  if(!Clear(muzzle,point,shot->target)){session.Skip();return;}
  const uint32_t base=stack.formId;
  const float damage=player.WeaponDamage(stack);
  const float cost=session.roundsFired?0:shot->ap;
  if(!player.FireVatsRound(session.weapon,cost)){Finish(true);return;}
  pose=AimPose(model,point);
  HeldShotEffects(player,weapon,muzzle,game);
  flashEnd=raw+std::max(.025f,float(flashUntil-game));
  const unsigned current=Chance(player,weapon,*part,muzzle,point,shot->target);
  if(Random()*100.f<float(std::min(shot->chance,current))){
    const auto region=fo3weapon::BoneRegion(part->target);
    ActorImpact(player,fo3player::PlayerRef,shot->target,base,damage,uint8_t(region),muzzle,&*part);
  }
  savedShot=true;
  Log("VATS FIRE",session.weapon,base);
  session.Fired();
  // Use the existing weapon cadence, bounded for comfortable VR playback.
  nextRound=raw+std::clamp(1.f/std::max(1.f,weapon.shotsPerSecond),.08f,.40f);
}

std::string Number(float number){return std::to_string(int(std::round(number)));}
const char* Alignment(uint8_t type){
  if(type==1||type==2||type==13)return "up";
  if(type==3||type==4||type==7||type==8||type==9)return "left";
  if(type==5||type==6||type==10||type==11||type==12)return "right";
  return "down";
}
void Prepare(const fo3player::Player& player){
  const auto label=[&](const char* key){return fo3menuui::Setting(player,key);};
  std::vector<std::string> queued;
  const auto& sheet=session.shots;
  for(size_t i=session.index;i<session.count;++i){
    const auto& shot=sheet[i];
    const auto* parts=player.ActorBodyParts(shot.target);
    if(!parts)continue;
    const auto part=std::find_if(parts->begin(),parts->end(),[&](const auto& value){return value.type==shot.part;});
    if(part!=parts->end())queued.push_back(label("sActionPointsShoot")+"  "+part->name);
  }
  const float maxAP=std::max(1.f,player.MaxActionPoints());
  const float ap=player.ActionPoints();
  const bool aiming=session.phase==fo3vats::Phase::Aiming;
  const float reserved=aiming?session.ReservedAP():0;
  const std::string apText=label("sActionPointsShort")+" "+Number(ap-reserved)+"/"+Number(maxAP);
  const std::string hpText=label("sHitPointsShort")+" "+Number(player.Health());
  const std::string notice=!status.empty()?status:view.target?std::string{}:label("sVatsAiming");
  auto key=fo3menuui::TextKey(view.name,fo3menuui::TextKey(apText+hpText+notice));
  for(const auto& line:queued)key=fo3menuui::TextKey(line,key);
  key^=uint64_t(aiming)|(uint64_t(std::max(0.f,view.health)*1000)<<1);
  if(chrome.revision!=key){
    chrome.Begin(player);
    auto& layout=chrome.layout;
    layout.Set("Select_button","string",label("sVatsSelect").empty()?"Select":label("sVatsSelect"));
    fo3menu::BindVatsChrome(layout,hpText,apText,player.Health()/std::max(1.f,player.MaxHealth()),
                           ap/maxAP,reserved/maxAP,view.name,view.health,queued,notice,aiming);
    chrome.Build();chrome.revision=key;
  }
  if(!aiming)return;
  for(const auto& limb:view.limbs){
    auto& doc=limbDocs[limb.part->type];
    if(!doc.Ensure("menus\\vats_menu.xml",420,260))continue;
    const bool selected=limb.part->type==view.selected;
    const auto state=fo3menuui::TextKey(limb.part->name)^
        (uint64_t(limb.chance)<<1)^(uint64_t(limb.condition*1000)<<9)^uint64_t(selected);
    if(doc.revision==state)continue;
    doc.Begin(player);
    const int row=fo3menu::BindVatsLimb(doc.layout,{limb.part->name,limb.chance,limb.condition,selected,Alignment(limb.part->type)});
    const auto textBounds=[&](const char* name){
      const int text=doc.layout.Find(name,row);
      auto box=doc.layout.Bounds(text);
      const int align=int(doc.layout.N(text,"justify"));
      box.x-=box.w*(align==2?1.f:align==1?.5f:0.f);
      return box;
    };
    auto bounds=doc.layout.Bounds(doc.layout.Find("meter",row));
    const auto include=[&](const fo3menu::Box& b){
      const float right=std::max(bounds.x+bounds.w,b.x+b.w);
      const float bottom=std::max(bounds.y+bounds.h,b.y+b.h);
      bounds.x=std::min(bounds.x,b.x);bounds.y=std::min(bounds.y,b.y);
      bounds.w=right-bounds.x;bounds.h=bottom-bounds.y;
    };
    include(textBounds("chance_to_hit"));
    if(selected)include(textBounds("limb_name"));
    limbLabelBounds[limb.part->type]=bounds;
    doc.Build();doc.revision=state;
  }
}
}

bool Fo3VatsFocus(){return vatsruntime::session.Active();}
void CancelFo3Vats(){vatsruntime::Finish(false);vatsruntime::input={};}
void ShutdownFo3Vats(){
  using namespace vatsruntime;
  Finish(false);
  input={};
  chrome.surface.Shutdown();chrome={};
  for(auto& doc:limbDocs){doc.surface.Shutdown();doc={};}
}

bool UpdateFo3Vats(double raw,double game,float lt,float rt,bool a,bool b,bool tracked){
  using namespace vatsruntime;
  using namespace weaponruntime;
  if(!tracked)input={};
  const auto edges=tracked?input.Sample(lt,rt,a,b):fo3vats::Edges{};
  const bool was=session.Active();
  if(!gPlayerSession||gPlayerSession->saveBlocked||!gSceneReady||IsFo3LoadingVisible()||
     Fo3GameplayBlockedOutsideVats()||gFo3GameplayInputSuppressed||!tracked||
     Fo3PipboyFocus()||Fo3BarterFocus()||Fo3DialogueFocus()||Fo3LockpickFocus()||Fo3TerminalFocus()||
     Fo3ScriptMessageFocus()||Fo3LevelUpFocus()||Fo3AppearanceFocus()||Fo3PlayerNameFocus()||
     Fo3SpecialBookFocus()||Fo3TagSkillsFocus()||Fo3MovieFocus()||gPlayerSeat.Active()){
    Finish(false);
    return was;
  }
  auto& player=gPlayerSession->player;
  const auto* stack=player.EquippedWeapon();
  if(!held||!rawHeldReady||!stack||stack->id!=active||WeaponReloadActive()||
     player.PlayerControlDisabled(2)||player.Health()<=0||gQ217FingerGrip[1]<=.25f){
    Finish(false);return was;
  }
  if(!was&&!edges.enter)return false;
  const auto model=weaponruntime::models.find(stack->formId);
  if(model==weaponruntime::models.end()||!model->second.ready){Finish(false);return was;}
  const auto& weapon=player.Definitions().items.at(stack->formId).weapon;
  if(!weapon.Firearm()){Finish(false);return was;}
  if(!was){
    if(!chrome.Ensure("menus\\vats_menu.xml",1280,900)){fo3audio::NamedSound("UIVATSEnterFail");return true;}
    PauseFo3ResidentGameplayTime();
    session.Open(stack->id);owner=&player;cell=gCurrentCellFormId;worldspace=gExteriorWorldspaceQ1890;
    generation=player.ScriptRuntimeGeneration();
    ResetFo3Pipboy();
    fo3audio::NamedSound("UIVATSEnter");
  }
  if(owner!=&player||session.weapon!=stack->id||cell!=gCurrentCellFormId||
     worldspace!=gExteriorWorldspaceQ1890||generation!=player.ScriptRuntimeGeneration()||edges.cancel){
    Finish(edges.cancel);return true;
  }
  pose=fo3anim::Multiply(RawPrimaryHand(),rawToWeapon);
  const auto& asset=model->second;
  if(session.phase==fo3vats::Phase::Aiming){
    const auto muzzle=Point(pose,GamePoint(asset.asset.muzzle));
    const auto direction=fo3vr::Unit(Direction(pose,GamePoint(asset.asset.muzzleForward)));
    const auto old=view.selected;
    const auto target=view.target;
    view=Target(player,weapon,muzzle,direction);
    if(view.target&&view.target!=target)fo3audio::NamedSound("UIVATSSelectTarget");
    else if(view.target&&view.selected!=old)fo3audio::NamedSound("UIVATSMove");
    if(edges.select)Queue(player,*stack,weapon,raw);
    if(edges.confirm&&session.Confirm()){
      nextRound=raw+player.Definitions().weapons.vats.playbackDelay;
      fo3audio::NamedSound("UIVATSReady");
    }
  }else{
    if(raw>=flashEnd)flashUntil=0;
    if(auto* actor=Actor(view.target))view.health=player.HealthPercentage(actor->source.refFormId);
    Execute(player,*stack,weapon,asset,raw,game);
  }
  if(raw>=statusUntil)status.clear();
  if(session.Active())Prepare(player);
  return true;
}

void RenderFo3Vats(const float* vp){
  using namespace vatsruntime;
  if(!session.Active())return;
  const float yaw=gQ210Head[3],c=std::cos(yaw),s=std::sin(yaw);
  const auto draw=[&](Document& doc,V anchor,float scale){
    const float model[16]{c,0,-s,0,0,1,0,0,s,0,c,0,anchor.x,anchor.y,anchor.z,1};
    float mvp[16]{};
    for(int col=0;col<4;++col)for(int row=0;row<4;++row)for(int k=0;k<4;++k)
      mvp[col*4+row]+=vp[k*4+row]*model[col*4+k];
    fo3menuui::WorldRegion(doc.surface,mvp,{0,0,doc.layout.width,doc.layout.height},scale);
  };
  const V head{gQ210Head[0],gQ210Head[1],gQ210Head[2]};
  draw(chrome,head+V{-s*1.28f,0,-c*1.28f},.001f);
  if(session.phase!=fo3vats::Phase::Aiming)return;
  // Use the centre view for both eyes so a readout has the same visibility in stereo.
  std::array<fo3menu::Box,14> occupied{};
  size_t count=0;
  for(bool selected:{true,false})for(const auto& limb:view.limbs){
    if((limb.part->type==view.selected)!=selected)continue;
    if(!selected&&std::any_of(view.limbs.begin(),view.limbs.end(),[&](const auto& other){
      return other.part->type==view.selected&&other.part->name==limb.part->name;
    }))continue;
    const V toward=limb.point-head;
    const float distance=fo3vr::Length(toward);
    const float depth=-s*toward.x-c*toward.z;
    if(depth<.15f)continue;
    auto& doc=limbDocs[limb.part->type];
    if(!doc.ready)continue;
    const V anchor=limb.point-toward*(.035f/distance);
    const float scale=depth*.00034f;
    const float z=depth*(1-.035f/distance);
    const auto bounds=limbLabelBounds[limb.part->type];
    const fo3menu::Box box{
        ((anchor.x-head.x)*c-(anchor.z-head.z)*s+(bounds.x-doc.layout.width*.5f)*scale)/z,
        (anchor.y-head.y+(doc.layout.height*.5f-bounds.y-bounds.h)*scale)/z,
        bounds.w*scale/z,bounds.h*scale/z};
    const auto end=occupied.begin()+count;
    if(!selected&&std::any_of(occupied.begin(),end,[&](const auto& b){
      constexpr float gap=.006f;
      return box.x<b.x+b.w+gap&&b.x<box.x+box.w+gap&&
             box.y<b.y+b.h+gap&&b.y<box.y+box.h+gap;
    }))continue;
    if(count==occupied.size())break;
    occupied[count++]=box;
    draw(doc,anchor,scale);
  }
}
