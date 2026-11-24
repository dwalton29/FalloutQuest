// Ordinary opaque NIF surfaces. Preserve directed light, local light radii,
// signed colours, authored tint, distance fog and alpha after the clip tests.
if (fqLightingMode == 3) {
    vec3 N=normalize(vNormal);
    float fade=1.0-smoothstep(6.0,18.0,distance(vPosition,uEyePosition));
    vec2 uvDx=dFdx(vUv),uvDy=dFdy(vUv);
    if (fade>0.0 && uNormalStrength>0.0) {
        vec3 detail=textureGrad(uNormalGloss,vUv,uvDx,uvDy).rgb*2.0-1.0;
        N=normalize(normalize(vTangent)*detail.x*fade+
                    normalize(vBitangent)*detail.y*fade+N*mix(1.0,detail.z,fade));
    }
    vec3 sun=normalize(uSunDirection);
    vec3 light=max(uAmbientColor+uSunlightColor*max(dot(N,sun),0.0),vec3(0));
    for(int i=0;i<8;++i) {
        if(i>=uLocalLightCount)break;
        vec3 delta=uLocalLightPosRadius[i].xyz-vPosition;
        float radius=max(uLocalLightPosRadius[i].w,.001);
        float dist2=dot(delta,delta);
        if(dist2>=radius*radius)continue;
        float dist=sqrt(max(dist2,1e-10));
        float strength=uInteriorMode!=0?1.0-dist2/(radius*radius):
            pow(max(0.0,1.0-dist/radius),max(uLocalLightColorFalloff[i].w,.25));
        light+=uLocalLightColorFalloff[i].rgb*(strength*max(dot(N,delta/dist),0.0));
    }
    fragColor=vec4(max(mix(baseColor*light,uFogColor,vFogFactorQ1532),vec3(0)),alpha);
    return;
}
