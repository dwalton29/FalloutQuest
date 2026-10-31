#pragma once
#include <cstdint>
namespace fo3lod {
// Original FO3 BSShaderFlags2: landscape=2, building=4. These native
// precombined materials are submitted through the engine's LOD pass.
inline bool Surface(bool native,uint32_t flags){return native&&(flags&6u)!=0;}
inline bool Cutout(bool native,uint32_t flags,bool authoredAlpha,bool realDiffuse,bool opaque){
 return native&&(flags&4u)!=0&&!authoredAlpha&&realDiffuse&&!opaque;
}
}
