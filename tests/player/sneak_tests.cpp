#include "player/fo3-sneak.h"
#include "npc/fo3-detection.h"
#include "player/fo3-vr-body.h"
#include "dialogue/fo3-dialogue-conditions.h"
#include <cassert>
#include <iostream>
using namespace fo3sneak;
int main(int argc,char** argv){
 Stance stance;stance.Step(1.7f,true,true,false,.02f);assert(!stance.sneaking);
 stance.Step(1.4f,true,true,false,.02f);assert(stance.sneaking&&stance.physical&&stance.offset==0);
 stance.Step(1.5f,true,true,false,.02f);assert(stance.physical);stance.Step(1.6f,true,true,false,.02f);assert(!stance.sneaking);
 stance.Step(1.7f,true,true,true,.02f);assert(stance.manual);for(int i=0;i<40;++i)stance.Step(1.7f,true,true,true,.02f);assert(stance.manual&&std::abs(stance.offset+.595f)<.001f);
 stance.Step(1.1f,true,true,false,.02f);assert(stance.sneaking&&stance.offset==0); // No double lowering.
 stance.Step(1.1f,false,true,true,.02f);assert(stance.manual); // Bad tracking cannot toggle.
 stance.Step(1.7f,true,false,false,.02f);stance.Step(1.7f,true,false,true,.02f);stance.Step(1.7f,true,true,true,.02f);assert(stance.manual); // Held through UI cannot toggle on exit.
 stance.Step(1.7f,true,true,false,.02f);stance.Step(1.7f,true,true,true,.02f);assert(!stance.manual);
 const auto leg=fo3vr::SolveCrouchLeg({0,1,0},{0,.5f,-.03f},{0,0,0},.5f);assert(leg.valid);
 assert(std::abs(fo3vr::Length(leg.knee-fo3vr::V{0,1,0})-fo3vr::Length({0,-.5f,-.03f}))<1e-4f);
 assert(fo3vr::Length(leg.thigh.Point({0,.5f,-.03f})-leg.knee)<1e-4f);assert(fo3vr::Length(leg.calf.Point({0,0,0})-leg.ankle)<1e-4f);assert(std::abs(leg.ankle.y-.5f)<1e-4f);
 auto settings=NativeSettings();Evidence e;e.distance=0;e.perception=5;e.light=0;const float normal=Value(settings,e);e.sneaking=true;e.skill=100;assert(std::abs(Value(settings,e)-(normal-50))<.001f);
 e.moving=true;const float moving=Value(settings,e);e.running=true;assert(Value(settings,e)>moving);e.action=100;assert(Value(settings,e)>moving);e.los=true;const auto sound=Value(settings,e);e.light=100;assert(Value(settings,e)>sound);e.distance=1e6f;assert(std::isinf(Value(settings,e)));
 Observer o;o.Step(settings,-100,true,0,{});assert(o.Hud()==Indicator::Hidden);o.Step(settings,-10,true,1,{1,2,3});assert(o.Hud()==Indicator::Caution);o.Step(settings,10,true,2,{2,3,4});assert(o.Hud()==Indicator::Danger);o.Step(settings,-100,true,3,{9,9,9});assert(o.Hud()==Indicator::Caution&&(o.lastKnown==std::array<float,3>{2,3,4}));o.Step(settings,-100,true,20,{});assert(o.Hud()==Indicator::Hidden);o.Step(settings,10,false,21,{});assert(o.Hud()==Indicator::Detected);
 Observer hit;hit.Hit(settings,30,{3,2,1});hit.Step(settings,-100,true,30,{});assert(hit.Hud()==Indicator::Caution&&(hit.lastKnown==std::array<float,3>{3,2,1}));hit.Step(settings,-100,true,50,{});assert(hit.Hud()==Indicator::Hidden);
 fo3player::Catalog c;c.initial.baseHealth=100;c.initial.special.fill(5);fo3player::Item item;item.formId=100;item.name="Fixture item";c.items[100]=item;c.pipboy.formNames["fixtureitem"]=100;
 fo3player::Player p(c);fo3dialogue::Context context;context.player=&p;context.speaker.reference=0x14;fo3pipdata::Condition cond;cond.function=286;cond.value=1;std::string error;
 assert(!fo3dialogue::Conditions({cond},context,error));p.SetSneaking(true);assert(fo3dialogue::Conditions({cond},context,error));
 fo3pipdata::ResultScript script;script.source="if Player.IsSneaking\nPlayer.AddItem FixtureItem 1\nendif";assert(p.PreviewDialogueResult(script,error)&&p.ItemCount(100)==0);assert(p.ExecuteDialogueResult(script,error)&&p.ItemCount(100)==1);p.SetSneaking(false);assert(p.ExecuteDialogueResult(script,error)&&p.ItemCount(100)==1);
 p.SetFurnitureUse(500,11,3);cond.function=159;cond.value=3;assert(fo3dialogue::Conditions({cond},context,error));cond.function=160;cond.value=11;assert(fo3dialogue::Conditions({cond},context,error));
 script.source="if Player.GetSitting == 3\nPlayer.AddItem FixtureItem 1\nendif";assert(p.PreviewDialogueResult(script,error)&&p.ItemCount(100)==1);assert(p.ExecuteDialogueResult(script,error)&&p.ItemCount(100)==2);p.SetFurnitureUse(0,0,0);assert(p.ExecuteDialogueResult(script,error)&&p.ItemCount(100)==2);
 if(argc>1){fo3player::Catalog original;assert(fo3player::LoadCatalog(argv[1],original,error));assert(std::abs(Setting(original.sneakSettings,"fMoveSneakMult")-.57f)<.001f);assert(Setting(original.sneakSettings,"fSneakBaseValue")==-35&&Setting(original.sneakSettings,"fSneakMaxDistance")==2500&&Setting(original.sneakSettings,"iSoundLevelSilent")==10);unsigned per=0;for(const auto& a:original.weapons.actors)per+=a.second.perception>0;assert(per>100);std::cout<<"Original ESM overrides and NPC perception passed actors="<<per<<"\n";}
 std::cout<<"Crouch input, leg retarget, detection formula/state, original conditions and script preview tests passed\n";
}
