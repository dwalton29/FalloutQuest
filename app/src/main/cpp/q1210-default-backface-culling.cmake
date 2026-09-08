# Q12.1: restore Fallout 3/Gamebryo default back-face culling for static NIFs.
#
# Ordinary FO3 NiTriShape/NiTriStrips geometry is single-sided unless an
# NiStencilProperty explicitly changes the draw mode. Q11.6 only touched culling
# when NiStencilProperty was present, leaving every ordinary shape at whatever
# GL_CULL_FACE state the surrounding renderer happened to have. In VR that is
# especially visible on thin signs: front/back faces can both rasterize and
# shimmer or produce incorrect lighting/specular as the head moves.
#
# Establish Gamebryo's normal single-sided state per draw, while preserving the
# existing Q11.6 NiStencilProperty override for authored two-sided/reversed draws.

set(Q1210_OLD_CULL_SETUP [==[
    GLboolean q1160CullWasEnabled = GL_FALSE;
    GLint q1160PreviousFrontFace = GL_CCW;
    GLint q1160PreviousCullMode = GL_BACK;
    if (object.stencilDrawModePresent) {
        q1160CullWasEnabled = glIsEnabled(GL_CULL_FACE);
        glGetIntegerv(GL_FRONT_FACE, &q1160PreviousFrontFace);
        glGetIntegerv(GL_CULL_FACE_MODE, &q1160PreviousCullMode);
        if (object.stencilDrawMode == 3u) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(object.stencilDrawMode == 2u ? GL_CW : GL_CCW);
        }
    }
]==])
set(Q1210_NEW_CULL_SETUP [==[
    const GLboolean q1160CullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLint q1160PreviousFrontFace = GL_CCW;
    GLint q1160PreviousCullMode = GL_BACK;
    glGetIntegerv(GL_FRONT_FACE, &q1160PreviousFrontFace);
    glGetIntegerv(GL_CULL_FACE_MODE, &q1160PreviousCullMode);

    // Gamebryo default: ordinary geometry is single-sided.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // NiStencilProperty is FO3's authored override for two-sided / reversed
    // face rendering. Preserve Q11.6's decoded draw mode exactly.
    if (object.stencilDrawModePresent) {
        if (object.stencilDrawMode == 3u) {
            glDisable(GL_CULL_FACE);
        } else {
            glFrontFace(object.stencilDrawMode == 2u ? GL_CW : GL_CCW);
        }
    }
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1210_OLD_CULL_SETUP}" Q1210_SETUP_POS)
if(Q1210_SETUP_POS EQUAL -1)
    message(FATAL_ERROR "Q12.1 could not find Q11.6 cull setup")
endif()
string(REPLACE "${Q1210_OLD_CULL_SETUP}" "${Q1210_NEW_CULL_SETUP}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1210_OLD_CULL_RESTORE [==[
    if (object.stencilDrawModePresent) {
        glFrontFace(static_cast<GLenum>(q1160PreviousFrontFace));
        glCullFace(static_cast<GLenum>(q1160PreviousCullMode));
        if (q1160CullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    }
]==])
set(Q1210_NEW_CULL_RESTORE [==[
    glFrontFace(static_cast<GLenum>(q1160PreviousFrontFace));
    glCullFace(static_cast<GLenum>(q1160PreviousCullMode));
    if (q1160CullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1210_OLD_CULL_RESTORE}" Q1210_RESTORE_POS)
if(Q1210_RESTORE_POS EQUAL -1)
    message(FATAL_ERROR "Q12.1 could not find Q11.6 cull restore")
endif()
string(REPLACE "${Q1210_OLD_CULL_RESTORE}" "${Q1210_NEW_CULL_RESTORE}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Add one targeted state log for the three known repro assets without spamming
# every static draw.
set(Q1210_OLD_BIND_VAO [==[
    glBindVertexArray(object.vao);

    const GLboolean q1160CullWasEnabled = glIsEnabled(GL_CULL_FACE);
]==])
set(Q1210_NEW_BIND_VAO [==[
    glBindVertexArray(object.vao);

    if (object.modelPath.find("megatonbrasslanternsign") != std::string::npos ||
        object.modelPath.find("megatonchurchofatom") != std::string::npos ||
        object.modelPath.find("signstop02") != std::string::npos) {
        Q6H_LOGI("Q12.1 CULL TARGET: model=%s stencil=%d mode=%u",
                 object.modelPath.c_str(), object.stencilDrawModePresent ? 1 : 0,
                 static_cast<unsigned>(object.stencilDrawMode));
    }

    const GLboolean q1160CullWasEnabled = glIsEnabled(GL_CULL_FACE);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1210_OLD_BIND_VAO}" Q1210_BIND_POS)
if(Q1210_BIND_POS EQUAL -1)
    message(FATAL_ERROR "Q12.1 could not find static VAO/cull hook")
endif()
string(REPLACE "${Q1210_OLD_BIND_VAO}" "${Q1210_NEW_BIND_VAO}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "Gamebryo default: ordinary geometry is single-sided" Q1210_CULL_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q12.1 CULL TARGET" Q1210_LOG_OK)
if(Q1210_CULL_OK EQUAL -1 OR Q1210_LOG_OK EQUAL -1)
    message(FATAL_ERROR "Q12.1 back-face culling verification failed")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q12.1 default Gamebryo back-face culling enabled")
