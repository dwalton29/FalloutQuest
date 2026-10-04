#pragma once
#include "fo3-interior-lighting.h"
#include "fo3-environment.h"
#include "fo3-imagespace.h"
#include "fo3-visual-depth.h"

inline fo3interior::Snapshot gFo3InteriorLighting;

// Called only by the render-thread scene commit while LSCR still covers the swap.
inline void PublishFo3InteriorLighting(fo3interior::Snapshot snapshot) {
    gFo3InteriorLighting=std::move(snapshot);
    const auto& s=gFo3InteriorLighting;const auto& c=s.cell;
    Fo3Environment env;
    env.interior=true;env.cellFormId=c.formId;
    // Black is the deliberate absence of authored light, not an exterior sun.
    for(int i=0;i<3;++i) {
        env.ambient[i]=c.authored?c.ambient[i]:0;
        env.sunlight[i]=c.authored?c.directional[i]:0;
        env.fog[i]=c.authored?c.fog[i]:0;
        env.horizon[i]=env.fog[i];env.skyUpper[i]=env.skyLower[i]=env.sun[i]=0;
    }
    env.fogNear=c.fogNear;env.fogFar=c.fogFar;
    // XCLL rotations are degrees in game coordinates. The scene maps (x,y,z)
    // to (x,z,-y), just as the geometry and placed lights do.
    constexpr float radians=3.14159265358979323846f/180.0f;
    float xy=c.rotationXY*radians,z=c.rotationZ*radians;
    env.sunDirection[0]=std::cos(xy)*std::cos(z);
    env.sunDirection[1]=std::sin(z);
    env.sunDirection[2]=-std::sin(xy)*std::cos(z);
    env.valid=true; // A missing XCLL remains a deliberately dark interior mode.
    gFo3Environment=env;
    ResetFo3PlacedLights();
    for(const auto& l:s.lights) {
        Fo3PlacedLight p;p.refFormId=l.ref;p.baseFormId=l.base;p.editorId=l.editorId;
        for(int i=0;i<3;++i){p.position[i]=l.position[i];p.color[i]=l.colour[i];}
        p.radius=l.radius;p.fade=l.fade;p.falloff=l.falloff;p.flags=l.flags;
        gFo3PlacedLights.push_back(std::move(p));
    }
    Fo3ImageSpace image;image.cellFormId=c.formId;image.imageSpaceFormId=c.imageSpace;
    image.imageSpaceFromCell=true;
    fo3imagespace::ParseImageSpacePayload(s.imagePayload,image);
    gFo3ImageSpace=std::move(image);
    fo3cellenv::gCellEnvironment={};
    fo3cellenv::gFogPower=c.authored?c.fogPower:1;
    gFo3RawWeatherLightingReady=false;
    fo3color::ResetState();
    __android_log_print(ANDROID_LOG_INFO,"FalloutQuest",
        "INTERIOR LIGHT READY cell=%08X EDID=%s found=%d XCLL=%d flags=%02X ambient=(%.6f %.6f %.6f) directional=(%.6f %.6f %.6f) rotation=(%d %d) dirFade=%.3f fog=(%.6f %.6f %.6f) fogNear=%.1f fogFar=%.1f fogClip=%.1f fogPower=%.3f LTMP=%08X inherit=%08X XCIM=%08X IMGS=%d refsRead=%d LIGHtotal=%zu active=%zu shaderBudget=%d selection=per-object-AABB unsupported=%zu unresolvedParents=%zu colour=normalized-editor-bytes attenuation=SP17-one-minus-squared-distance",
        c.formId,c.editorId.c_str(),c.found,c.authored,c.flags,
        env.ambient[0],env.ambient[1],env.ambient[2],env.sunlight[0],env.sunlight[1],env.sunlight[2],
        c.rotationXY,c.rotationZ,c.directionalFade,env.fog[0],env.fog[1],env.fog[2],
        c.fogNear,c.fogFar,c.fogClip,c.fogPower,c.lightingTemplate,c.inherit,c.imageSpace,
        gFo3ImageSpace.valid,s.referencesRead,s.total,s.lights.size(),FO3_SHADER_LIGHTS,s.unsupported,s.unresolvedParents);
}
