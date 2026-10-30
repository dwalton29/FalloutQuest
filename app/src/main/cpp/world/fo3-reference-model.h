#pragma once
#include <cstring>
namespace fo3world {
// ARMO MODL/MOD3 are worn biped meshes; MOD2 is the authored default world
// model. Placed references must not instantiate a skinned outfit/gore rig.
inline bool PlacedModelField(const char* recordType,const char* subrecord){
 return std::memcmp(subrecord,std::memcmp(recordType,"ARMO",4)==0?"MOD2":"MODL",4)==0;
}
}
