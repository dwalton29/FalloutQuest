#include "npc/fo3-npc-conversation.h"
#include <cassert>
#include <iostream>
#include <string>

static fo3player::Catalog Fixture(bool enabled=true){
  fo3player::Catalog c;c.initial.baseHealth=100;
  c.weapons.actors[0xA0].health=90;
  c.weapons.actors[0xB0].health=90;
  c.pipboy.targets[0xA1].base=0xA0;c.pipboy.targets[0xA1].cell=0x100;
  c.pipboy.targets[0xB1].base=0xB0;c.pipboy.targets[0xB1].cell=0x100;
  auto& topic=c.pipboy.dialogueTopics[0xD2];topic.editor="HELLO";topic.type=1;
  auto& quest=c.pipboy.quests[0xC1];quest.flags=enabled?1:0;quest.priority=20;
  fo3pipdata::Info info;
  info.id=0xD3;info.topic=0xD2;info.quest=0xC1;info.speaker=0xA0;
  info.responses.push_back({0xDD,1,"Good afternoon."});
  c.pipboy.topics[0xD2].push_back(std::move(info));
  c.pipboy.sounds[0xDD]="@sound:authored-npc-hello";
  return c;
}
static fo3dialogue::Context Context(const fo3player::Player& player){
  fo3dialogue::Context ctx;ctx.player=&player;
  ctx.speaker={0xA1,0xA0,0x100,0,0,0,0,0,false,false};
  ctx.target={0xB1,0xB0,0x100,0,0,80,0,0,false,false};
  return ctx;
}
int main(){
  {
    fo3player::Player player(Fixture());
    const auto& d=player.Definitions().pipboy;
    const auto ctx=Context(player);
    const auto line=fo3npcchat::SelectHello(d,player,ctx);
    assert(line.Valid()&&line.info==0xD3&&line.speaker==0xA1&&line.listener==0xB1);
    assert(line.subtitle=="Good afternoon."&&line.audio=="@sound:authored-npc-hello");
    auto other=ctx;other.target.reference=0x14;
    assert(!fo3npcchat::SelectHello(d,player,other).Valid());
    other=ctx;other.target.reference=0xA1;
    assert(!fo3npcchat::SelectHello(d,player,other).Valid());
    other=ctx;other.target.combat=true;
    assert(!fo3npcchat::SelectHello(d,player,other).Valid());
    other=ctx;other.speaker.base=0xB0;
    assert(!fo3npcchat::SelectHello(d,player,other).Valid());
  }
  {
    fo3player::Player player(Fixture(false));
    assert(!fo3npcchat::SelectHello(player.Definitions().pipboy,player,Context(player)).Valid());
  }
  for(int k=0;k<4;++k){
    auto c=Fixture();
    auto& info=c.pipboy.topics[0xD2][0];
    if(k==0)info.begin.source="SetStage MS11 10";
    if(k==1)info.end.compiled={0x12};
    if(k==2)info.nextSpeaker=1;
    if(k==3)info.orderValid=false;
    fo3player::Player player(std::move(c));
    assert(!fo3npcchat::SelectHello(player.Definitions().pipboy,player,Context(player)).Valid());
  }
  std::cout<<"NPC HELLO: original INFO/voice, real NPC listener, quest-gated, script-free and interruption eligibility passed\n";
  return 0;
}
