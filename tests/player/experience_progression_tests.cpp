#include "player/fo3-experience.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>

static void Check(bool ok, const char* reason) {
  if (!ok) throw std::runtime_error(reason);
}
int main() {
  try {
    using namespace fo3experience;
    Check(ThresholdForLevel(1)==0,"level 1");
    Check(ThresholdForLevel(2)==200,"level 2");
    Check(ThresholdForLevel(3)==550,"level 3");
    Check(ThresholdForLevel(4)==1050,"level 4");
    Check(ThresholdForLevel(5)==1700,"level 5");
    Check(ThresholdForLevel(10)==7200,"level 10");
    Check(ThresholdForLevel(20)==29450,"level 20 cap");
    Check(ThresholdForLevel(30)==66700,"DLC curve");
    Check(ThresholdForLevel(5,175)==1850,"authored XP bump");
    Check(ThresholdForLevel(UINT16_MAX,UINT32_MAX)==UINT32_MAX,"overflow");
    auto p=Evaluate(1,199);
    Check(!p.ready&&p.pendingLevels==0&&p.nextThreshold==200,"before threshold");
    p=Evaluate(1,200);
    Check(p.ready&&p.pendingLevels==1&&p.level==1,"no silent level-up");
    p=Evaluate(1,1050);
    Check(p.pendingLevels==3&&p.level==1,"multiple pending");
    p=Evaluate(3,1050);
    Check(p.pendingLevels==1&&p.currentThreshold==550&&p.nextThreshold==1050,
          "committed levels");
    p=Evaluate(20,40000);
    Check(!p.ready&&p.pendingLevels==0&&p.nextThreshold==29450,"level cap");
    p=Evaluate(1,UINT32_MAX);
    Check(p.pendingLevels==19&&p.level==1,"large XP capped");
    p=Evaluate(1,550,150,2);
    Check(p.pendingLevels==1&&p.maxLevel==2,"selected cap");
    std::cout<<"Experience progression tests passed\n";
    return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
