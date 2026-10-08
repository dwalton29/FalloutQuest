#include "npc/fo3-unloaded-ai.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#define main XtelFixtureMain
#include "../world/xtel_index_tests.cpp"
#undef main

namespace {
fo3xtel::Index Graph() {
    // Use the same two authored-format interior/exterior XTEL pairs as the
    // v185 index tests, but enable the originally disabled destination door.
    Bytes original=Master();
    bool cleared=false;
    for(size_t at=0;at+24<=original.size();++at){
        if(std::memcmp(original.data()+at,"REFR",4)!=0)continue;
        const auto ref=uint32_t(original[at+12])|(uint32_t(original[at+13])<<8)|
            (uint32_t(original[at+14])<<16)|(uint32_t(original[at+15])<<24);
        if(ref==0x102){U32(original,at+8,0);cleared=true;break;}
    }
    assert(cleared);
    const auto path=Write(original);
    fo3xtel::Index result;std::string err;
    assert(result.Build(path,err)&&err.empty());
    std::remove(path.c_str());
    return result;
}
struct Fixture {
  fo3pipdata::Definitions defs;
  fo3xtel::Index graph=Graph();
  std::unordered_map<uint32_t,fo3player::ActorState> tracked;
  fo3unloaded::Scheduler scheduler;
  uint32_t base=0x3000,actor=0x22;
  bool doorOpen=true,eligible=true,alive=true;
  int writes=0;
  Fixture() {
    fo3pipdata::Placement ref;
    ref.base=base;ref.cell=0x200;ref.x=200;ref.y=200;
    defs.targets.emplace(actor,ref);
    fo3pipdata::Placement door;
    door.base=0x1000;door.cell=0x200;door.x=11;door.y=12;
    defs.targets.emplace(0x101,door);
    door.cell=0x300;door.world=0x700;
    defs.targets.emplace(0x102,door);
    fo3pipdata::Placement destination;
    destination.cell=0x300;destination.world=0x700;
    destination.x=150;destination.y=200;
    defs.targets.emplace(0x777,destination);
    fo3pipdata::Placement home;
    home.cell=0x200;home.x=300;home.y=350;
    defs.targets.emplace(0x778,home);
    fo3pipdata::PackageDefinition work;
    work.type=6;work.location={0,0x777,0,true};
    work.schedule={-1,-1,8,0,12,true};
    defs.packages.emplace(0x900,work);
    fo3pipdata::PackageDefinition sleep=work;
    sleep.type=4;sleep.location.value=0x778;
    sleep.schedule={-1,-1,20,0,12,true};
    defs.packages.emplace(0x901,sleep);
    fo3player::ActorState initial;
    initial.cell=0x200;initial.world=0;initial.position={300,350,25};
    tracked.emplace(actor,initial);
  }
  fo3unloaded::Callbacks Callbacks(){
    fo3unloaded::Callbacks cb;
    cb.packages=[&](uint32_t) {return std::vector<uint32_t>{0x900,0x901};};
    cb.eligible=[&](uint32_t,uint32_t,const fo3player::ActorState&,
                    const fo3pipdata::PackageDefinition&,float){return eligible;};
    cb.canUseDoor=[&](uint32_t,uint32_t){return doorOpen;};
    cb.alive=[&](uint32_t){return alive;};
    cb.persist=[&](uint32_t who,const fo3player::ActorState& s){
      ++writes;tracked[who]=s;return true;
    };
    return cb;
  }
  fo3unloaded::Report Tick(float hour,uint32_t cell=0x99,uint32_t world=0){
    const auto callbacks=Callbacks();
    return scheduler.Tick(hour,cell,world,tracked,defs,graph,callbacks);
  }
};
void TestDayNight() {
  Fixture f;
  assert(f.Tick(7.99f).doorHops==0); // Startup does not fabricate elapsed time.
  auto r=f.Tick(8.01f);
  assert(r.doorHops==1&&r.packageChanges==1);
  auto s=f.tracked.at(f.actor);
  assert(s.cell==0x300&&s.world==0x700&&s.package==0x900);
  assert((s.position==std::array<float,3>{11,12,13}));
  assert(s.yaw==.375f);
  assert(f.Tick(8.01f).doorHops==0&&f.writes==1); // No duplicate hop in minute.
  assert(f.Tick(8.02f).doorHops==0); // Already at destination; no teleport.
  r=f.Tick(20.01f);
  assert(r.packageChanges==1&&r.doorHops==1);
  s=f.tracked.at(f.actor);
  assert(s.cell==0x200&&s.package==0x901);
  // Bed entry is not simulated while the interior is unloaded.
  assert((s.position==std::array<float,3>{11,12,13}));
  assert(f.Tick(23.99f).doorHops==0);
  assert(f.Tick(0.01f).doorHops==0);
  assert(f.tracked.at(f.actor).package==0x901);
  r=f.Tick(8.01f);assert(r.doorHops==1&&r.packageChanges==1);
  std::cout<<"Original-format doors and 08:00/20:00 overnight package changes passed\n";
}
void TestGuards() {
  Fixture f;
  f.Tick(7.99f);
  f.doorOpen=false;
  auto r=f.Tick(8.01f);
  assert(r.blocked==1&&f.writes==0);
  f.doorOpen=true;
  f.Tick(8.02f);
  assert(f.writes==1);
  f.Tick(8.03f,0x300,0x700); // Resident actor never remotely simulated.
  assert(f.writes==1);
  f.alive=false;
  f.Tick(20.01f);assert(f.writes==1);
  f.alive=true;f.tracked.at(f.actor).hostile=0x14;
  f.Tick(20.02f);assert(f.writes==1);
  f.tracked.at(f.actor).hostile=0;
  f.eligible=false;f.Tick(20.03f);assert(f.writes==1);
  f.eligible=true;f.Tick(20.04f);assert(f.writes==2);
  f.tracked.at(f.actor).dead=true;
  f.Tick(8.01f);assert(f.writes==2);
  std::cout<<"Disabled doors, resident exclusion, health, hostility and condition gates passed\n";
}
void TestUnsupported() {
  Fixture f;f.Tick(7.99f);
  f.defs.packages.at(0x900).scripted=true;
  f.defs.packages.at(0x901).location2.valid=true;
  f.Tick(8.01f);assert(f.writes==0);
  f.defs.packages.at(0x900).scripted=false;
  f.defs.packages.at(0x900).schedule.month=10;
  f.Tick(8.02f);assert(f.writes==0);
  f.defs.packages.at(0x900).schedule.month=-1;
  f.defs.targets.at(0x777).cell=0;f.defs.targets.at(0x777).world=0;
  f.Tick(8.03f);assert(f.writes==0);
  std::cout<<"Scripts, dated schedules, unknown locations fail closed\n";
}
}
int main(){
  TestDayNight();TestGuards();TestUnsupported();
  return 0;
}
