# Q10.3: direct exterior boot.
# Q10.0 still used MegatonPlayerHouse as a hidden bootstrap before swapping to
# MegatonEntrance. Remove that bootstrap: create the GLES program only, then let
# the existing Q7.4 exterior scene-swap path load the authored gate XTEL as the
# first scene, collision world and LAND origin.

set(Q1030_DIRECT_BOOT_HELPER [=[
bool Q1030InitializeRenderProgramOnly() {
    if (gProgram) return true;
    gProgram = CreateQ6HProgram();
    if (!gProgram) {
        Q6H_LOGE("Q10.3 DIRECT BOOT FAILED: reason=shader-program");
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

    const bool coreReady = gMvpLocation >= 0 && gDiffuseLocation >= 0 &&
                           gNormalLocation >= 0 && gAmbientColorLocationQ1000 >= 0 &&
                           gSunlightColorLocationQ1000 >= 0 && gSunDirectionLocationQ1000 >= 0;
    Q6H_LOGI("Q10.3 RENDERER BOOT: program=%u coreUniforms=%d source=empty-no-house-bootstrap",
             gProgram, coreReady ? 1 : 0);
    return coreReady;
}

]=])
string(REPLACE
    "void Q6HGenFramebuffers(GLsizei n, GLuint* framebuffers) {"
    "${Q1030_DIRECT_BOOT_HELPER}void Q6HGenFramebuffers(GLsizei n, GLuint* framebuffers) {"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

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
    static bool bootAttemptedQ1030 = false;
    if (bootAttemptedQ1030) return;
    bootAttemptedQ1030 = true;

    if (!Q1030InitializeRenderProgramOnly()) return;
    if (!QueueFo3MegatonEntryQ1000()) {
        Q6H_LOGE("Q10.3 DIRECT BOOT FAILED: authored Megaton gate XTEL not found");
        return;
    }
    if (!ProcessQ74TransitionRequest()) {
        Q6H_LOGE("Q10.3 DIRECT BOOT FAILED: gate exterior scene load failed");
        return;
    }

    Q6H_LOGI("Q10.3 DIRECT MEGATON ENTRY READY: cell=00002DBD worldspace=00000A74 bootstrapCell=NONE source=Fallout3.esm/XTEL");
}
]=])
string(REPLACE "${Q1030_OLD_BOOT}" "${Q1030_NEW_BOOT}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Final renderer source after the direct-boot transform.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
