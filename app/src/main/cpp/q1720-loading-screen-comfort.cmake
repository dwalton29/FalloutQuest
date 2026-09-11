# Q16.2: comfortable Fallout 3 loading presentation for VR.
#
# Q16.0 proved real LSCR selection/rendering but presents the selected DDS as a
# very large post-composite eye overlay. In-headset that is visually oppressive
# and can feel uncomfortably close. Q16.2 keeps every loading-state/CELL/XTEL
# decision intact and changes only presentation:
#
#   - shrink and raise the authored loading slide so it reads as a distant panel
#   - keep the panel at zero forced stereo disparity (no convergence fight)
#   - add Fallout 3's familiar green clock/compass-style loading indicator below
#   - animate the indicator continuously while the loading state is visible
#
# Fallout 3's original animated UI asset is meshes\interface\loading\loadinganim01.nif.
# FalloutQuest does not yet execute Gamebryo UI NIF animation controllers, so the
# small clock/compass indicator is reproduced procedurally in the loading shader.
# The LSCR artwork itself is still the real selected Fallout3.esm ICON DDS.

set(Q1720_LOADING_SOURCE_INPUT
    "${CMAKE_CURRENT_SOURCE_DIR}/fo3-loading-screen-q1700.h")
if(NOT EXISTS "${Q1720_LOADING_SOURCE_INPUT}")
    message(FATAL_ERROR "Q16.2 expected Q16.0 loading renderer header")
endif()
file(READ "${Q1720_LOADING_SOURCE_INPUT}" Q1720_LOADING_SOURCE)

# Smooth animation time for the procedural loading clock.
string(REPLACE
    "#include <algorithm>\n#include <cstdint>"
    "#include <algorithm>\n#include <chrono>\n#include <cstdint>"
    Q1720_LOADING_SOURCE "${Q1720_LOADING_SOURCE}")

set(Q1720_UNIFORMS_OLD [==[
inline GLint gImageLoc = -1;
inline GLint gScaleLoc = -1;
]==])
set(Q1720_UNIFORMS_NEW [==[
inline GLint gImageLoc = -1;
inline GLint gScaleLoc = -1;
inline GLint gCenterLoc = -1;
inline GLint gViewportLoc = -1;
inline GLint gTimeLoc = -1;
]==])
string(FIND "${Q1720_LOADING_SOURCE}" "${Q1720_UNIFORMS_OLD}" Q1720_UNIFORMS_POS)
if(Q1720_UNIFORMS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.2 could not find loading uniform declarations")
endif()
string(REPLACE "${Q1720_UNIFORMS_OLD}" "${Q1720_UNIFORMS_NEW}"
       Q1720_LOADING_SOURCE "${Q1720_LOADING_SOURCE}")

# Replace Q16.0's image-only fragment path with a smaller centered slide plus the
# procedural vanilla-style loading clock/compass beneath it. The framebuffer is
# still filled black in one pass, preserving the existing loading-state coverage.
set(Q1720_FS_OLD [==[
    static const char* fsSource = R"(
        #version 300 es
        precision highp float;
        in vec2 vUv;
        uniform sampler2D uImage;
        uniform vec2 uScale;
        out vec4 fragColor;
        void main() {
            vec2 centered = vUv - vec2(0.5);
            vec2 halfScale = max(uScale * 0.5, vec2(0.0001));
            if (abs(centered.x) > halfScale.x || abs(centered.y) > halfScale.y) {
                fragColor = vec4(0.0, 0.0, 0.0, 1.0);
                return;
            }
            vec2 uv = centered / uScale + vec2(0.5);
            vec4 image = texture(uImage, uv);
            fragColor = vec4(image.rgb * image.a, 1.0);
        }
    )";
]==])
set(Q1720_FS_NEW [==[
    static const char* fsSource = R"(
        #version 300 es
        precision highp float;
        in vec2 vUv;
        uniform sampler2D uImage;
        uniform vec2 uScale;
        uniform vec2 uCenter;
        uniform vec2 uViewport;
        uniform float uTime;
        out vec4 fragColor;

        float sdSegment(vec2 p, vec2 a, vec2 b) {
            vec2 pa = p - a;
            vec2 ba = b - a;
            float h = clamp(dot(pa, ba) / max(dot(ba, ba), 0.000001), 0.0, 1.0);
            return length(pa - ba * h);
        }

        float stroke(float distanceToLine, float halfWidth, float feather) {
            return 1.0 - smoothstep(halfWidth, halfWidth + feather, distanceToLine);
        }

        void main() {
            vec3 colour = vec3(0.0);
            vec2 halfScale = max(uScale * 0.5, vec2(0.0001));
            vec2 panel = vUv - uCenter;

            // Real Fallout3.esm LSCR artwork, deliberately smaller and slightly
            // above centre so the loading indicator has its own breathing room.
            if (abs(panel.x) <= halfScale.x && abs(panel.y) <= halfScale.y) {
                vec2 uv = panel / uScale + vec2(0.5);
                vec4 image = texture(uImage, uv);
                colour = image.rgb * image.a;
            }

            // Fallout 3 loadinganim01 visual language: minimal clock face with a
            // continuously rotating compass/needle. X is aspect-corrected so the
            // dial stays circular in each eye framebuffer.
            float aspect = uViewport.x / max(uViewport.y, 1.0);
            vec2 spinnerCenter = vec2(uCenter.x, uCenter.y - halfScale.y - 0.070);
            vec2 p = vUv - spinnerCenter;
            p.x *= aspect;

            const float PI = 3.14159265359;
            const float TAU = 6.28318530718;
            const float radius = 0.031;
            float core = 0.0;
            float glow = 0.0;

            // Outer clock ring.
            float ringDistance = abs(length(p) - radius);
            core = max(core, stroke(ringDistance, 0.00115, 0.00115));
            glow = max(glow, stroke(ringDistance, 0.0048, 0.0030));

            // Twelve dial marks, with the four cardinal marks slightly longer.
            float angle = atan(p.y, p.x);
            float sector = TAU / 12.0;
            float tickAngle = floor((angle + sector * 0.5) / sector) * sector;
            float tickIndex = mod(floor((angle + PI + sector * 0.5) / sector), 12.0);
            float cardinal = 1.0 - step(0.1, mod(tickIndex, 3.0));
            vec2 tickDir = vec2(cos(tickAngle), sin(tickAngle));
            float tickInner = mix(radius - 0.0050, radius - 0.0082, cardinal);
            float tickDistance = sdSegment(p, tickDir * tickInner,
                                           tickDir * (radius - 0.0008));
            core = max(core, stroke(tickDistance, 0.00095, 0.0011));
            glow = max(glow, stroke(tickDistance, 0.0038, 0.0028));

            // Rotating compass needle / clock hand. One long luminous pointer and
            // a short counterweight match the read of the original endless dial.
            float spin = -uTime * 2.55 + PI * 0.5;
            vec2 needleDir = vec2(cos(spin), sin(spin));
            float needleDistance = sdSegment(p, -needleDir * 0.0080,
                                             needleDir * 0.0240);
            core = max(core, stroke(needleDistance, 0.00125, 0.0010));
            glow = max(glow, stroke(needleDistance, 0.0052, 0.0030));

            // Small hub at the centre of the dial.
            float hubDistance = abs(length(p) - 0.0032);
            core = max(core, stroke(hubDistance, 0.0010, 0.0010));
            glow = max(glow, stroke(hubDistance, 0.0042, 0.0028));

            // Fallout 3 HUDMain green: RGB(26,255,128), with a restrained glow.
            vec3 hud = vec3(26.0 / 255.0, 1.0, 128.0 / 255.0);
            colour += hud * glow * 0.14;
            colour = mix(colour, hud, clamp(core, 0.0, 1.0));

            fragColor = vec4(clamp(colour, 0.0, 1.0), 1.0);
        }
    )";
]==])
string(FIND "${Q1720_LOADING_SOURCE}" "${Q1720_FS_OLD}" Q1720_FS_POS)
if(Q1720_FS_POS EQUAL -1)
    message(FATAL_ERROR "Q16.2 could not find Q16.0 loading fragment shader")
endif()
string(REPLACE "${Q1720_FS_OLD}" "${Q1720_FS_NEW}"
       Q1720_LOADING_SOURCE "${Q1720_LOADING_SOURCE}")

set(Q1720_UNIFORM_LOOKUP_OLD [==[
    gImageLoc = glGetUniformLocation(gProgram, "uImage");
    gScaleLoc = glGetUniformLocation(gProgram, "uScale");
    return gImageLoc >= 0 && gScaleLoc >= 0;
]==])
set(Q1720_UNIFORM_LOOKUP_NEW [==[
    gImageLoc = glGetUniformLocation(gProgram, "uImage");
    gScaleLoc = glGetUniformLocation(gProgram, "uScale");
    gCenterLoc = glGetUniformLocation(gProgram, "uCenter");
    gViewportLoc = glGetUniformLocation(gProgram, "uViewport");
    gTimeLoc = glGetUniformLocation(gProgram, "uTime");
    return gImageLoc >= 0 && gScaleLoc >= 0 && gCenterLoc >= 0 &&
           gViewportLoc >= 0 && gTimeLoc >= 0;
]==])
string(FIND "${Q1720_LOADING_SOURCE}" "${Q1720_UNIFORM_LOOKUP_OLD}" Q1720_LOOKUP_POS)
if(Q1720_LOOKUP_POS EQUAL -1)
    message(FATAL_ERROR "Q16.2 could not find loading uniform lookup block")
endif()
string(REPLACE "${Q1720_UNIFORM_LOOKUP_OLD}" "${Q1720_UNIFORM_LOOKUP_NEW}"
       Q1720_LOADING_SOURCE "${Q1720_LOADING_SOURCE}")

# Q16.0 uses 86% of each eye's width. Q16.2 deliberately drops that to 62%,
# caps tall slides more tightly, raises the panel, and drives the animated dial.
# Keeping the panel at identical eye coordinates avoids forcing convergence and
# is substantially more comfortable than a giant eye-filling overlay.
set(Q1720_RENDER_SCALE_OLD [==[
        const float viewAspect = static_cast<float>(width) / static_cast<float>(height);
        const float imageAspect = static_cast<float>(gImageWidth) /
                                  static_cast<float>(std::max(gImageHeight, 1));
        float sx = 0.86f;
        float sy = sx * viewAspect / std::max(imageAspect, 0.001f);
        if (sy > 0.78f) {
            sy = 0.78f;
            sx = sy * imageAspect / std::max(viewAspect, 0.001f);
        }
        glUniform2f(gScaleLoc, std::clamp(sx, 0.05f, 0.95f),
                    std::clamp(sy, 0.05f, 0.95f));
        glDrawArrays(GL_TRIANGLES, 0, 3);
]==])
set(Q1720_RENDER_SCALE_NEW [==[
        const float viewAspect = static_cast<float>(width) / static_cast<float>(height);
        const float imageAspect = static_cast<float>(gImageWidth) /
                                  static_cast<float>(std::max(gImageHeight, 1));
        float sx = 0.62f;
        float sy = sx * viewAspect / std::max(imageAspect, 0.001f);
        if (sy > 0.48f) {
            sy = 0.48f;
            sx = sy * imageAspect / std::max(viewAspect, 0.001f);
        }
        glUniform2f(gScaleLoc, std::clamp(sx, 0.05f, 0.80f),
                    std::clamp(sy, 0.05f, 0.60f));
        glUniform2f(gCenterLoc, 0.50f, 0.60f);
        glUniform2f(gViewportLoc, static_cast<float>(width), static_cast<float>(height));
        static const auto q1720Start = std::chrono::steady_clock::now();
        const float q1720Seconds = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - q1720Start).count();
        glUniform1f(gTimeLoc, q1720Seconds);
        glDrawArrays(GL_TRIANGLES, 0, 3);
]==])
string(FIND "${Q1720_LOADING_SOURCE}" "${Q1720_RENDER_SCALE_OLD}" Q1720_SCALE_POS)
if(Q1720_SCALE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.2 could not find Q16.0 loading scale block")
endif()
string(REPLACE "${Q1720_RENDER_SCALE_OLD}" "${Q1720_RENDER_SCALE_NEW}"
       Q1720_LOADING_SOURCE "${Q1720_LOADING_SOURCE}")

# Make generated/log identity explicit while preserving the Q1700 public API.
string(REPLACE "Q16.0 LOADING" "Q16.2 LOADING"
       Q1720_LOADING_SOURCE "${Q1720_LOADING_SOURCE}")
string(REPLACE "Q16.0 LSCR" "Q16.2 LSCR"
       Q1720_LOADING_SOURCE "${Q1720_LOADING_SOURCE}")

set(Q1720_LOADING_GENERATED
    "${CMAKE_CURRENT_BINARY_DIR}/fo3-loading-screen-q1720-generated.h")
file(WRITE "${Q1720_LOADING_GENERATED}" "${Q1720_LOADING_SOURCE}")

# Both generated render translation units inherited the Q1700 header include.
# Point both at the same Q16.2 generated header so inline loading globals retain
# one definition across the final binary.
set(Q1720_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
set(Q1720_Q6H_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1720_Q4_INPUT}" OR NOT EXISTS "${Q1720_Q6H_INPUT}")
    message(FATAL_ERROR "Q16.2 expected final Q4 and Q6H generated sources")
endif()
file(READ "${Q1720_Q4_INPUT}" Q1720_Q4_SOURCE)
file(READ "${Q1720_Q6H_INPUT}" Q1720_Q6H_SOURCE)

string(REPLACE
    "#include \"fo3-loading-screen-q1700.h\""
    "#include \"fo3-loading-screen-q1720-generated.h\""
    Q1720_Q4_SOURCE "${Q1720_Q4_SOURCE}")
string(REPLACE
    "#include \"fo3-loading-screen-q1700.h\""
    "#include \"fo3-loading-screen-q1720-generated.h\""
    Q1720_Q6H_SOURCE "${Q1720_Q6H_SOURCE}")

# Visible build proof: Q16.1 -> Q16.2 on the left controller.
set(Q1720_LABEL_OLD [==[
        q1600Digit(q1600X, 0x06u); // Q16.1: 1 = B C
]==])
set(Q1720_LABEL_NEW [==[
        q1600Digit(q1600X, 0x5Bu); // Q16.2: 2 = A B G E D
]==])
string(FIND "${Q1720_Q4_SOURCE}" "${Q1720_LABEL_OLD}" Q1720_LABEL_POS)
if(Q1720_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.2 could not find Q16.1 final build-label digit")
endif()
string(REPLACE "${Q1720_LABEL_OLD}" "${Q1720_LABEL_NEW}"
       Q1720_Q4_SOURCE "${Q1720_Q4_SOURCE}")
string(REPLACE "Q16.1" "Q16.2" Q1720_Q4_SOURCE "${Q1720_Q4_SOURCE}")

file(WRITE "${Q1720_Q4_INPUT}" "${Q1720_Q4_SOURCE}")
file(WRITE "${Q1720_Q6H_INPUT}" "${Q1720_Q6H_SOURCE}")

# Configure-time assertions prevent a build that silently falls back to Q16.0.
string(FIND "${Q1720_LOADING_SOURCE}" "float sx = 0.62f;" Q1720_SIZE_OK)
string(FIND "${Q1720_LOADING_SOURCE}" "uTime * 2.55" Q1720_SPINNER_OK)
string(FIND "${Q1720_LOADING_SOURCE}" "26.0 / 255.0" Q1720_GREEN_OK)
string(FIND "${Q1720_Q4_SOURCE}" "fo3-loading-screen-q1720-generated.h" Q1720_Q4_INCLUDE_OK)
string(FIND "${Q1720_Q6H_SOURCE}" "fo3-loading-screen-q1720-generated.h" Q1720_Q6H_INCLUDE_OK)
string(FIND "${Q1720_Q4_SOURCE}" "Q16.2: 2 = A B G E D" Q1720_LABEL_OK)
if(Q1720_SIZE_OK EQUAL -1 OR Q1720_SPINNER_OK EQUAL -1 OR
   Q1720_GREEN_OK EQUAL -1 OR Q1720_Q4_INCLUDE_OK EQUAL -1 OR
   Q1720_Q6H_INCLUDE_OK EQUAL -1 OR Q1720_LABEL_OK EQUAL -1)
    message(FATAL_ERROR
        "Q16.2 verification failed: size=${Q1720_SIZE_OK} spinner=${Q1720_SPINNER_OK} green=${Q1720_GREEN_OK} q4=${Q1720_Q4_INCLUDE_OK} q6h=${Q1720_Q6H_INCLUDE_OK} label=${Q1720_LABEL_OK}")
endif()

message(STATUS "Q16.2 loading comfort enabled: 62% centered LSCR + animated HUDMain clock/compass below")
