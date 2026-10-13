#ifndef FO3_LOCKPICK_HEADLESS
#include "fo3-lockpick-renderer.h"
#endif
#include "ui/interaction/fo3-item-notifications.h"
#include <random>
fo3lockpick::Game gLockpick;
bool gLockpickInputLatch=false;
bool Fo3LockpickInputBlocked(){return gLockpickInputLatch||Fo3LockpickFocus();}
bool gLockpickLastA=true,gLockpickLastB=true,gLockpickReleased=false,gLockpickLoop=false;
double gLockpickTime=0,gLockpickSoundTime=0,gLockpickBreakStarted=0,gLockpickEntered=0;
std::mt19937 gLockpickRandom(std::random_device{}());
bool Fo3LockpickFocus(){return gLockpick.reference!=0;}
void CancelFo3Lockpick(){gLockpick={};fo3lockui::anchored=false;gLockpickReleased=false;fo3lockui::breakElapsed=-1;
    if(gLockpickLoop)fo3audio::MenuLoop("");gLockpickLoop=false;}
std::string LockString(const char* key,const char* original){return fo3lockui::String(gPlayerSession->player,key,original);}
bool BeginFo3Lockpick(uint32_t reference){
    if(!gPlayerSession||gPlayerSession->saveBlocked)return false;
    auto& p=gPlayerSession->player;const auto access=p.CanPickLock(reference);
    switch(access){
      case fo3player::PickAccess::RequiresKey:fo3notify::Notifications().Enqueue(LockString("sImpossibleLock","This lock cannot be picked. It requires a key to open."));return false;
      case fo3player::PickAccess::NoPins:fo3notify::Notifications().Enqueue(LockString("sNoLockPick","You don't have any bobbypins."));return false;
      case fo3player::PickAccess::Broken:fo3notify::Notifications().Enqueue(LockString("sLockBroken","The lock has been broken."));return false;
      case fo3player::PickAccess::Skill:{char text[256];std::snprintf(text,sizeof(text),LockString("sLockpickSkillTooLow","You need a lockpick skill of %d to pick this lock.").c_str(),p.LockRequirement(reference));fo3notify::Notifications().Enqueue(text);return false;}
      case fo3player::PickAccess::Ready:break;
      default:return false;
    }
    fo3lockui::Advance();if(!fo3lockui::ready)return false;
    if(!gLockpick.Begin(p,reference,std::generate_canonical<float,24>(gLockpickRandom)))return false;
    gLockpickInputLatch=true;gLockpickLastA=gLockpickLastB=true;gLockpickReleased=false;gLockpickTime=0;fo3lockui::introElapsed=0;fo3lockui::anchored=false;
    p.EndContainerInteraction();fo3audio::NamedSound("UILockpickingEnter");return true;
}
void UpdateFo3Lockpick(double now,float motion,float torque,bool a,bool b,bool focused,const float* head,bool gripsDown){
    fo3lockui::Advance();if(!Fo3LockpickFocus()){if(!a&&!b&&torque<.1f&&!gripsDown)gLockpickInputLatch=false;return;}
    if(!focused||!gPlayerSession||gPlayerSession->saveBlocked||IsFo3LoadingVisible()){CancelFo3Lockpick();return;}
    fo3lockui::SetHead(head);auto& p=gPlayerSession->player;
    const bool pressA=a&&!gLockpickLastA,pressB=b&&!gLockpickLastB;gLockpickLastA=a;gLockpickLastB=b;
    if(gLockpickTime==0)gLockpickEntered=now;fo3lockui::introElapsed=float(now-gLockpickEntered);
    const float dt=gLockpickTime>0?float(now-gLockpickTime):0;gLockpickTime=now;
    if(!gLockpickReleased){gLockpickReleased=!a&&!b&&torque<.1f&&std::fabs(motion)<.2f;return;}
    if(pressB){CancelFo3Lockpick();return;}
    auto intro=std::find_if(fo3lockui::assets.lock.clips.begin(),fo3lockui::assets.lock.clips.end(),[](const auto& c){return c.name=="Forward";});
    if(intro!=fo3lockui::assets.lock.clips.end()&&fo3lockui::introElapsed<intro->stop-intro->start)return;
    const auto reference=gLockpick.reference;
    if(fo3lockui::breakElapsed>=0){fo3lockui::breakElapsed=float(now-gLockpickBreakStarted);
      auto clip=std::find_if(fo3lockui::assets.pin.clips.begin(),fo3lockui::assets.pin.clips.end(),[](const auto& c){return c.name=="Left";});
      const float duration=clip==fo3lockui::assets.pin.clips.end()?0:clip->stop-clip->start;
      if(fo3lockui::breakElapsed>=duration){
        if(p.ItemCount(p.Definitions().bobbyPin)>0){gLockpick.Replace();gLockpickReleased=false;fo3lockui::breakElapsed=-1;}
        else {fo3notify::Notifications().Enqueue(LockString("sOutOfLockpicks","You are out of lockpicks."));CancelFo3Lockpick();}}
      return;
    }
    if(p.CanPickLock(reference)!=fo3player::PickAccess::Ready){CancelFo3Lockpick();return;}
    if(pressA){if(p.ForceLock(reference,std::uniform_int_distribution<uint32_t>(0,99)(gLockpickRandom))){
      const bool success=!p.GetLock(reference).locked;fo3audio::NamedSound(success?"UILockpickingUnlock":"UILockpickingForceFail");
      if(!success)fo3notify::Notifications().Enqueue(LockString("sLockBroken","The lock has been broken."));
      gWeaponHaptic[1]=success?.22f:.28f;FlushFo3PlayerState();CancelFo3Lockpick();}return;}
    const float previousPin=gLockpick.pin,previousCylinder=gLockpick.cylinder;
    const auto event=gLockpick.Step(dt,motion,torque);
    if(event==fo3lockpick::Game::Event::Unlock){if(p.CompleteLockpick(reference)){
      fo3audio::NamedSound("UILockpickingUnlock");gWeaponHaptic[1]=.22f;FlushFo3PlayerState();}
      CancelFo3Lockpick();return;}
    if(event==fo3lockpick::Game::Event::Break){if(p.BreakBobbyPin(reference)){
      fo3audio::NamedSound("UILockpickingPickBreak");gWeaponHaptic[1]=.3f;FlushFo3PlayerState();
      gLockpickBreakStarted=now;fo3lockui::breakElapsed=0;
      fo3audio::MenuLoop("");gLockpickLoop=false;}
      return;}
    if(event==fo3lockpick::Game::Event::Bind){fo3audio::NamedSound("UILockpickingCylinderStop");gWeaponHaptic[1]=.12f;}
    if(gLockpick.binding!=gLockpickLoop){gLockpickLoop=gLockpick.binding;fo3audio::MenuLoop(gLockpickLoop?"UILockpickingPickTensionLPM":"");}
    if(now-gLockpickSoundTime>.18){
      if(gLockpick.binding) {fo3audio::NamedSound(gLockpick.durability<.5f?"UILockpickingCylinderSqueakB":"UILockpickingCylinderSqueakA");gWeaponHaptic[1]=.08f;gLockpickSoundTime=now;}
      else if(std::fabs(gLockpick.pin-previousPin)>.01f){fo3audio::NamedSound("UILockpickingPickMovement");gLockpickSoundTime=now;}
      else if(std::fabs(gLockpick.cylinder-previousCylinder)>.01f){fo3audio::NamedSound("UILockpickingCylinderTurn");gLockpickSoundTime=now;}
    }
}
