#pragma once
#include "gl-state-cache.h"
#include "quest-quality-policy.h"
#include <EGL/egl.h>
#include <android/log.h>
#include <sys/system_properties.h>
#include <cstdlib>

namespace fqfoveation {
// QCOM_shading_rate preserves per-pixel coverage/depth, unlike scaled-bin
// framebuffer foveation. Do not apply that older extension to the HDR target.
using RateFn = void (*)(GLenum);
constexpr GLenum RateState=0x96A4, FullRate=0x96A6, EdgeRate=0x96A9;
inline RateFn rate=nullptr;
inline bool initialized=false;
inline fqquality::Rect viewport{}, scissor{};
inline bool scissored=false;
inline GLint savedRate=FullRate;
inline GLenum currentRate=FullRate;
inline bool HasExtension(const char* all,const char* name) {
    if(!all)return false;
    const size_t length=std::strlen(name);
    for(const char* p=all;(p=std::strstr(p,name));p+=length)
        if((p==all||p[-1]==' ')&&(p[length]==' '||p[length]=='\0'))return true;
    return false;
}
inline void Initialize() {
    if(initialized)return;
    initialized=true;
    char value[PROP_VALUE_MAX]{};
    __system_property_get("debug.falloutquest.foveation",value);
    const bool disabled=value[0] && std::atoi(value)==0;
    if(!disabled && HasExtension(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)),
                                 "GL_QCOM_shading_rate"))
        rate=reinterpret_cast<RateFn>(eglGetProcAddress("glShadingRateQCOM"));
    __android_log_print(ANDROID_LOG_INFO,"FalloutQuest",
        "Q240 FIXED FOVEATION: available=%d disabled=%d mode=QCOM-shading-rate centre=70pct edge=2x2 largeDrawMin=3072 scope=main-world depth=per-pixel",
        rate!=nullptr,disabled);
}
inline void SetRate(GLenum next) {
    if(currentRate!=next){rate(next);currentRate=next;}
}
inline unsigned Count(GLsizei n) {
    // Avoid multiplying tiny draws and blended terrain layers. Keep the
    // original alpha-pass ordering and all UI at full rate.
    return n>=3072 && fqgl::state.blend.known && !fqgl::state.blend.value ? 5 : 1;
}
inline bool Select(unsigned index) {
    auto r=fqquality::Region(index,viewport);
    if(scissored)r=fqquality::Intersect(r,scissor);
    if(r.w<=0||r.h<=0)return false;
    SetRate(index==0?FullRate:EdgeRate);
    glEnable(GL_SCISSOR_TEST);glScissor(r.x,r.y,r.w,r.h);
    return true;
}
inline void Restore() {
    SetRate(FullRate);
    glScissor(scissor.x,scissor.y,scissor.w,scissor.h);
    if(!scissored)glDisable(GL_SCISSOR_TEST);
}
struct Scope {
    bool active=false;
    Scope() {
        Initialize();
        if(!rate || fqgl::drawRegionCount)return;
        GLint v[4]{},s[4]{};
        glGetIntegerv(GL_VIEWPORT,v);glGetIntegerv(GL_SCISSOR_BOX,s);
        glGetIntegerv(RateState,&savedRate);
        currentRate=static_cast<GLenum>(savedRate);
        viewport={v[0],v[1],v[2],v[3]};scissor={s[0],s[1],s[2],s[3]};
        scissored=glIsEnabled(GL_SCISSOR_TEST);
        SetRate(FullRate);
        fqgl::drawRegionCount=Count;fqgl::selectDrawRegion=Select;fqgl::restoreDrawRegion=Restore;
        active=true;
    }
    ~Scope() {
        if(!active)return;
        Restore();SetRate(static_cast<GLenum>(savedRate));
        fqgl::drawRegionCount=nullptr;fqgl::selectDrawRegion=nullptr;fqgl::restoreDrawRegion=nullptr;
    }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
}
