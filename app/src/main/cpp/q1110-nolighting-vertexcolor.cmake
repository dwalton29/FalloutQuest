# Q11.1: preserve authored vertex colours on textureless NoLighting geometry.
#
# Device Q11.0 logs proved the recurring Megaton fallback surfaces are not
# borrowing the wrong texture anymore. They are authored BSShaderNoLightingProperty
# shapes with an empty File Name and a full vertex-colour stream. nif.xml confirms
# that File Name is the direct field on BSShaderNoLightingProperty for FO3, so an
# empty value here is real authored data rather than parser drift.
#
# The renderer's historical diffuse fallback is tan (190,170,135). Q10.2 then
# multiplies that fallback by the authored vertex colour, tinting every genuinely
# untextured NoLighting surface brown. For only this proven case, use neutral white
# so the vertex colour passes through unchanged. Keep the old tan diagnostic
# fallback for every other missing diffuse texture.
#
# Use a distinct fallback label because empty-path textures are cached by label;
# otherwise a previously cached tan DIFFUSE fallback could be reused here.

set(Q1110_OLD_DIFFUSE_UPLOAD [==[
    if (!UploadTexture(cpu.mesh.diffuseTexturePath, {190u, 170u, 135u, 255u},
                       gpu.diffuse, gpu.realDiffuse, "DIFFUSE", gpu.refFormId)) return false;
]==])

set(Q1110_NEW_DIFFUSE_UPLOAD [==[
    const bool q1110NoLightingVertexColorOnly =
        cpu.mesh.noLighting &&
        cpu.mesh.diffuseTexturePath.empty() &&
        cpu.mesh.vertexColors.size() == vertexCount * 4u;
    if (q1110NoLightingVertexColorOnly) {
        if (!UploadTexture(cpu.mesh.diffuseTexturePath, {255u, 255u, 255u, 255u},
                           gpu.diffuse, gpu.realDiffuse,
                           "DIFFUSE_NOLIGHT_VCOLOR", gpu.refFormId)) return false;
        Q6H_LOGI("Q11.1 NOLIGHT VCOLOR: ref=%08X model=%s vertices=%zu diffuse=<white-neutral>",
                 gpu.refFormId, cpu.placement.modelPath.c_str(), vertexCount);
    } else {
        if (!UploadTexture(cpu.mesh.diffuseTexturePath, {190u, 170u, 135u, 255u},
                           gpu.diffuse, gpu.realDiffuse, "DIFFUSE", gpu.refFormId)) return false;
    }
]==])

string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1110_OLD_DIFFUSE_UPLOAD}" Q1110_UPLOAD_POS)
if(Q1110_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q11.1 could not find diffuse fallback upload hook")
endif()
string(REPLACE "${Q1110_OLD_DIFFUSE_UPLOAD}" "${Q1110_NEW_DIFFUSE_UPLOAD}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "DIFFUSE_NOLIGHT_VCOLOR" Q1110_LABEL_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q11.1 NOLIGHT VCOLOR" Q1110_LOG_OK)
if(Q1110_LABEL_OK EQUAL -1 OR Q1110_LOG_OK EQUAL -1)
    message(FATAL_ERROR "Q11.1 NoLighting vertex-colour fallback verification failed")
endif()

# Q10.7 wrote this generated source earlier in the milestone chain; Q11.1 is the
# final native-renderer mutation, so persist the updated source again here.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q11.1 neutral NoLighting vertex-colour fallback enabled")
