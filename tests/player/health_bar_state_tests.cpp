#include "ui/interaction/fo3-health-bar-state.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
  fo3health::Tracker hud;
  hud.player.Observe(200,200,0);
  assert(hud.player.Alpha(0)==0);
  hud.player.Observe(150,200,1);
  assert(hud.player.Alpha(1)==1&&std::fabs(hud.player.Fraction()-.75f)<.00001f);
  hud.player.Observe(165,200,1.1); // healing does not independently reopen
  assert(hud.player.Alpha(2.5)==1);
  assert(hud.player.Alpha(2.75)>.49f&&hud.player.Alpha(2.75)<.51f);
  assert(hud.player.Alpha(3.1)==0);
  hud.player.Observe(130,200,4);
  assert(hud.player.Alpha(4.1)==1); // repeated damage extends
  hud.ObserveActor(42,75,100,0);
  hud.ObserveActor(43,100,100,0);
  hud.ObserveActor(42,25,100,1);
  assert(hud.Actor(42)&&hud.Actor(42)->Alpha(1)>0);
  assert(hud.Actor(43)&&hud.Actor(43)->Alpha(1)==0);
  hud.ObserveActor(42,0,100,2);
  assert(hud.Actor(42)->Fraction()==0&&hud.Actor(42)->Alpha(2.1)==1);
  assert(hud.Actor(42)->Alpha(4.1)==0);
  hud.Clear();
  assert(!hud.Actor(42)&&hud.player.Alpha(5)==0);
  std::cout<<"Damage-only player and NPC health, original time and scene reset passed\n";
}
