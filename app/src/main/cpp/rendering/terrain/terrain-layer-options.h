#pragma once
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
namespace fo3terrain {
// A shader rejected by the device must never remove the original LAND path.
inline bool fusionProgramFailed = false;
inline void DisableFusionAfterProgramFailure() { fusionProgramFailed = true; }
// Only the registered native material may admit 15-float fused geometry.
// GLES retains its recovered original program, irrespective of old properties.
inline bool nativeFusionReady=false;
inline bool NativeFusionRequested() {
#ifdef __ANDROID__
    char value[PROP_VALUE_MAX]{};
    __system_property_get("debug.falloutquest.terrain_fusion",value);
    return !(value[0]=='0'&&value[1]=='\0');
#else
    return true;
#endif
}
inline bool LegacyLayers() { return !nativeFusionReady||fusionProgramFailed; }
inline bool UnsharedLighting() {
    static const bool unshared=[](){
#if defined(__ANDROID__) && !defined(NDEBUG)
        char value[PROP_VALUE_MAX]{};
        __system_property_get("debug.falloutquest.terrain_unshared",value);
        return value[0]=='1'&&value[1]=='\0';
#else
        return false;
#endif
    }();
    return unshared;
}
} // namespace fo3terrain
