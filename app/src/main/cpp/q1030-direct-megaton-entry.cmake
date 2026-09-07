# Q10.3: direct exterior boot.
# Q10.0 still used MegatonPlayerHouse as a hidden bootstrap before swapping to
# MegatonEntrance. Remove that bootstrap. The authored exterior is now built on
# the first real render callback, after the GLES/OpenXR render path is live.

set(Q1030_DIRECT_BOOT_HELPER [=[
bool Q1030InitializeRenderProgramOnly() {
    if (gProgram) return true;
    gProgram = CreateQ6HProgram();
    if (!gProgram) {
        Q6H_LOGE("Q10.3 DIRECT BOOT FAILED: stage=renderer reason=shader-program");
        return false;
    }

    gMvpLocation = glGetUniformLocation(gProgram, "uMvp");
    gDiffuseLocation = glGetUniformLocation(gProgram, "uDiffuse");
    gNormalLocation = glGetUniformLocation(gProgram, "uNormalGloss");
    gGlossinessLocation = glGetUniformLocation(gProgram, "uGlossiness");
    gNormalStrengthLocation = glGetUniformLocation(gProgram, "uNormalStrength");
    gMaterialAlphaLocation = glGetUniformLocation(gProgram, "uMaterialAlpha");
    gAlphaTestLocation = glGetUniformLocation(gProgram, "uAlphaTest");
    gAlphaThresholdLocation = glGetUniformLocation(gProgram, "uAlphaThreshold");

    gAmbientColorLocationQ1000 = glGetUniformLocation(gProgram, "uAmbientColor");
    gSunlightColorLocationQ1000 = glGetUniformLocation(gProgram, "uSunlightColor");
    gSunDirectionLocationQ1000 = glGetUniformLocation(gProgram, "uSunDirection");

    gEyePositionLocationQ1010 = glGetUniformLocation(gProgram, "uEyePosition");
    gFogColorLocationQ1010 = glGetUniformLocation(gProgram, "uFogColor");
    gFogNearLocationQ1010 = glGetUniformLocation(gProgram, "uFogNear");
    gFogFarLocationQ1010 = glGetUniformLocation(gProgram, "uFogFar");
    gLocalLightCountLocationQ1010 = glGetUniformLocation(gProgram, "uLocalLightCount");
    gLocalLightPosRadiusLocationQ1010 = glGetUniformLocation(gProgram, "uLocalLightPosRadius[0]");
    gLocalLightColorFalloffLocationQ1010 = glGetUniformLocation(gProgram, "uLocalLightColorFalloff[0]");

    gGlowLocationQ1020 = glGetUniformLocation(gProgram, "uGlow");
    gNoLightingLocationQ1020 = glGetUniformLocation(gProgram, "uNoLighting");
    gUseVertexColorLocationQ1020 = glGetUniformLocation(gProgram, "uUseVertexColor");
    gUseVertexAlphaLocationQ1020 = glGetUniformLocation(gProgram, "uUseVertexAlpha");
    gSpecularEnabledLocationQ1020 = glGetUniformLocation(gProgram, "uSpecularEnabled");
    gSpecularColorLocationQ1020 = glGetUniformLocation(gProgram, "uSpecularColor");
    gEmissiveColorLocationQ1020 = glGetUniformLocation(gProgram, "uEmissiveColor");
    gEmissiveMultLocationQ1020 = glGetUniformLocation(gProgram, "uEmissiveMult");
    gGlowEnabledLocationQ1020 = glGetUniformLocation(gProgram, "uGlowEnabled");

    // Only the transform and diffuse sampler are mandatory to build/draw the
    // scene. GLES may legally optimise optional material uniforms to -1.
    const bool coreReady = gMvpLocation >= 0 && gDiffuseLocation >= 0;
    Q6H_LOGI("Q10.3 RENDERER READY: program=%u core=%d mvp=%d diffuse=%d normal=%d ambient=%d sun=%d source=no-house-bootstrap",
             gProgram, coreReady ? 1 : 0, gMvpLocation, gDiffuseLocation,
             gNormalLocation, gAmbientColorLocationQ1000, gSunDirectionLocationQ1000);
    return coreReady;
}

bool Q1030BootMegatonOnRender() {
    static bool attempted = false;
    static bool succeeded = false;
    if (succeeded) return true;
    if (attempted) return false;
    attempted = true;

    Q6H_LOGI("Q10.3 DIRECT BOOT BEGIN: targetCell=00002DBD worldspace=00000A74 stage=first-render");
    if (!Q1030InitializeRenderProgramOnly()) return false;
    if (!QueueFo3MegatonEntryQ1000()) {
        Q6H_LOGE("Q10.3 DIRECT BOOT FAILED: stage=xtel reason=gate-not-found");
        return false;
    }
    Q6H_LOGI("Q10.3 DIRECT BOOT XTEL READY: targetCell=00002DBD source=Fallout3.esm");
    if (!ProcessQ74TransitionRequest()) {
        Q6H_LOGE("Q10.3 DIRECT BOOT FAILED: stage=exterior-load reason=scene-swap");
        return false;
    }

    succeeded = gSceneReady && !gObjects.empty();
    Q6H_LOGI("Q10.3 DIRECT MEGATON ENTRY READY: cell=00002DBD worldspace=00000A74 objects=%zu sceneReady=%d bootstrapCell=NONE source=Fallout3.esm/XTEL",
             gObjects.size(), gSceneReady ? 1 : 0);
    return succeeded;
}

]=])

# Put the direct boot helper immediately before RenderScene. At this point the
# Q7.4 scene-swap implementation and all Q10.x material helpers already exist.
string(REPLACE
    "void RenderScene() {"
    "${Q1030_DIRECT_BOOT_HELPER}void RenderScene() {"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# The first Q4 render trigger calls RenderScene even with no FO3 scene loaded.
# Use that trigger to perform the exterior build, then continue through the
# ordinary renderer in the same frame.
string(REPLACE
    "void RenderScene() {\n    ProcessQ74TransitionRequest();"
    "void RenderScene() {\n    if (!gSceneReady) Q1030BootMegatonOnRender();\n    ProcessQ74TransitionRequest();"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Q10.0's framebuffer hook still initialises MegatonPlayerHouse. Strip it down
# to framebuffer creation only. No house meshes/collision/scene are ever built.
set(Q1030_OLD_BOOT [=[
void Q6HGenFramebuffers(GLsizei n, GLuint* framebuffers) {
    glGenFramebuffers(n, framebuffers);
    const bool bootstrapReady = InitializeScene();
    if (bootstrapReady) {
        if (QueueFo3MegatonEntryQ1000()) {
            if (!ProcessQ74TransitionRequest()) {
                Q6H_LOGE("Q10.0 BOOT ENTRY FAILED: authored gate queued but exterior swap failed");
            }
        } else {
            Q6H_LOGE("Q10.0 BOOT ENTRY FAILED: authored Megaton gate XTEL not found; bootstrap scene retained");
        }
    }
}
]=])
set(Q1030_NEW_BOOT [=[
void Q6HGenFramebuffers(GLsizei n, GLuint* framebuffers) {
    glGenFramebuffers(n, framebuffers);
    static bool logged = false;
    if (!logged) {
        logged = true;
        Q6H_LOGI("Q10.3 FRAMEBUFFER READY: exteriorBoot=deferred-to-first-render bootstrapCell=NONE");
    }
}
]=])
string(REPLACE "${Q1030_OLD_BOOT}" "${Q1030_NEW_BOOT}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Final renderer source after the direct-boot transform.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
