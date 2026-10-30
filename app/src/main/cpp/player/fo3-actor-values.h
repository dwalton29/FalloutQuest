#pragma once
#include <cstdint>
#include <string>
namespace fo3av {
inline uint32_t ID(const std::string& name){
 static const char* special[]={"strength","perception","endurance","charisma","intelligence","agility","luck"};
 static const char* skills[]={"barter","bigguns","energyweapons","explosives","lockpick","medicine","meleeweapons","repair","science","smallguns","sneak","speech","throwing","unarmed"};
 static const char* conditions[]={"perceptioncondition","endurancecondition","leftattackcondition","rightattackcondition","leftmobilitycondition","rightmobilitycondition"};
 for(uint32_t i=0;i<7;++i)if(name==special[i])return 5+i;
 for(uint32_t i=0;i<14;++i)if(name==skills[i])return 32+i;
 for(uint32_t i=0;i<6;++i)if(name==conditions[i])return 25+i;
 if(name=="health")return 16;
 if(name=="actionpoints")return 12;
 if(name=="radiationrads")return 54;
 if(name=="karma")return 23;
 if(name=="inventoryweight")return 46;
 return UINT32_MAX;
}
}
