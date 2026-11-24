#pragma once
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
namespace fqquality {
inline bool WorldMaterials() {
#ifdef __ANDROID__
    static const bool enabled=[] {
        char value[PROP_VALUE_MAX]{};
        __system_property_get("debug.falloutquest.world_materials",value);
        return value[0]!='0';
    }();
    return enabled;
#else
    return true;
#endif
}
}
