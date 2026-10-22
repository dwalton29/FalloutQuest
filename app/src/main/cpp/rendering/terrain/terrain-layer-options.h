#pragma once
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
namespace fo3terrain {
// A shader rejected by the device must never remove the original LAND path.
inline bool fusionProgramFailed = false;
inline void DisableFusionAfterProgramFailure() { fusionProgramFailed = true; }
inline bool LegacyLayers() {
    static const bool legacy=[](){
#if defined(__ANDROID__) && !defined(NDEBUG)
        char value[PROP_VALUE_MAX]{};
        __system_property_get("debug.falloutquest.terrain_legacy",value);
        return value[0]=='1'&&value[1]=='\0';
#else
        return false;
#endif
    }();
    return legacy || fusionProgramFailed;
}
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
