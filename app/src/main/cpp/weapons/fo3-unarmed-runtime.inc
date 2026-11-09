#include "weapons/fo3-unarmed-gesture.h"

namespace fo3unarmed {
static std::array<Hand,2> fists;
static Hand batSwing;
static std::array<fo3vr::V,3> previousBatPoints;
static uint64_t batInstance=0;
static bool batSupport=false;
static uint64_t scene=0,session=0;
static float previousYaw=0;
}

void ResetFo3Unarmed(){
  fo3unarmed::fists={};
  fo3unarmed::batSwing.Reset();fo3unarmed::batInstance=0;
}
bool Fo3ClosedFist(size_t hand){return fo3unarmed::fists[hand].Closed();}

void UpdateFo3Unarmed(double now,float yaw,const std::array<std::optional<fo3vr::V>,2>& tracking){
  using namespace fo3unarmed;
  if(!gPlayerSession||gPlayerSession->saveBlocked||!gSceneReady||IsFo3LoadingVisible()||
     gFo3GameplayInputSuppressed||!gQ210PlayerBodyReady){
    ResetFo3Unarmed();return;
  }
  auto& player=gPlayerSession->player;
  if(player.PlayerControlDisabled(2)||player.Health()<=0){ResetFo3Unarmed();return;}
  const auto generation=player.ScriptRuntimeGeneration();
  if(scene!=gSceneLoad.generation||session!=generation||std::abs(fo3vr::Wrap(yaw-previousYaw))>.1f){
    scene=gSceneLoad.generation;session=generation;ResetFo3Unarmed();
  }
  previousYaw=yaw;

  if(const auto* bat=player.EquippedWeapon();bat&&weaponruntime::held&&tracking[1]&&
     gWeaponPalmValid[1]&&(!weaponruntime::support||tracking[0])&&
     player.Definitions().items.at(bat->formId).weapon.TwoHandMelee()&&bat->condition>0){
    const auto model=weaponruntime::models.find(bat->formId);
    if(model!=weaponruntime::models.end()&&model->second.ready){
      if(batInstance!=bat->id||batSupport!=weaponruntime::support){
        batSwing.Reset();batInstance=bat->id;batSupport=weaponruntime::support;
      }
      const fo3vr::V head{gQ210Head[0],gQ210Head[1],gQ210Head[2]};
      std::array<fo3vr::V,3> points;
      for(size_t i=0;i<points.size();++i)
        points[i]=weaponruntime::Point(weaponruntime::pose,model->second.strikePoints[i])-head;
      // Remove stick locomotion and invalidate samples on snap turns and grip changes.
      const auto swing=batSwing.Advance(now,gQ217FingerGrip[1],points.front());
      if(swing)for(size_t i=0;i<points.size();++i){
        const auto travel=points[i]-previousBatPoints[i];
        const float length=fo3vr::Length(travel);
        if(length<.005f||length>.40f)continue;
        if(weaponruntime::Hit(bat->id,head+previousBatPoints[i],fo3vr::Unit(travel),length,
                             bat->formId,player.WeaponDamage(bat->id))){
          batSwing.Impact(*swing);
          weaponruntime::MeleeHaptics(1,true,weaponruntime::support);
          break;
        }
      }
      previousBatPoints=points;
    }
  }else {batSwing.Reset();batInstance=0;}

  const auto& defs=player.Definitions();
  const auto form=defs.pipboy.formNames.find("fists");
  if(form==defs.pipboy.formNames.end())return;
  const auto item=defs.items.find(form->second);
  if(item==defs.items.end()||!item->second.weapon.valid||item->second.weapon.animation!=0)return;
  const float damage=player.ActorUnarmedDamage(fo3player::PlayerRef);
  if(damage<=0)return;
  const float scale=Fo3PlayerTrackingScale();
  const auto rotation=fo3vr::Axis({0,1,0},yaw);
  for(size_t hand=0;hand<fists.size();++hand){
    const bool occupied=hand==1?weaponruntime::held:
      weaponruntime::support||weaponruntime::magazine||weaponruntime::charging||weaponruntime::animatedReload;
    const auto palm=VrPoint(gWeaponPalm[hand]);
    if(occupied||gQ220Grab[hand].active||!tracking[hand]||!gWeaponPalmValid[hand]||!fo3vr::Finite(palm)){
      fists[hand].Reset();continue;
    }
    const auto motion=fists[hand].Advance(now,gQ217FingerGrip[hand],*tracking[hand]);
    if(!motion)continue;
    // Build the sweep from physical controller motion and the solved palm.
    // Stick locomotion is excluded. The short tip reaches beyond the palm to
    // the knuckles, while the shared hit trace gives walls and doors priority.
    const auto travelled=fo3vr::Rotate(rotation,*motion)*scale;
    if(weaponruntime::Hit(0,palm-travelled,fo3vr::Unit(travelled),
        fo3vr::Length(travelled)+.04f*scale,form->second,damage)){
      fists[hand].Impact(*motion);
      fo3audio::NamedSound("WPNHandSwing");
      weaponruntime::MeleeHaptics(int(hand),false);
    }
  }
}
