#pragma once
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
namespace fo3terrain {
// A shader rejected by the device must never remove the original LAND path.
inline bool fusionProgramFailed = false;
inline void DisableFusionAfterProgramFailure() { fusionProgramFailed = true; }
inline bool LegacyLayers() {
    // Q243 recovery: the fused program links on Adreno 740 but the reported
    // scene has no visible LAND, even with zero folded groups. Compile/link
    // fallback cannot detect this. Use the retained original shader AND keep
    // original layer geometry on cold and streamed admissions in every build.
    // In particular, an earlier terrain_legacy=0 must not re-enable this path.
    // Fusion remains quarantined until its device rendering failure is fixed.
    return true;
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
