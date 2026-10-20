#pragma once
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
namespace fo3terrain {
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
    return legacy;
}
} // namespace fo3terrain
