#include "weapons/fo3-unarmed-gesture.h"

namespace fo3unarmed {
static std::array<Hand,2> fists;
static uint64_t scene=0,session=0;
static float previousYaw=0;
}

void ResetFo3Unarmed(){fo3unarmed::fists={};}
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
      weaponruntime::Pulse(int(hand),.35f);
    }
  }
}
