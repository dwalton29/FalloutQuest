# Q15.10+: visible in-headset build identity.
#
# Draw a small vector label reading the active test version just above the left
# Touch controller. This is deliberately part of the rendered VR scene rather
# than logcat so every test immediately proves which APK is actually running.
# The label inherits the authored left-hand pose/MVP, uses the existing tiny
# OpenXR line-colour shader, and draws with GL_ALWAYS so world geometry cannot
# hide the build identifier.
#
# Q12.8 moved the live OpenXR eye code out of q4-native.cpp into the generated
# q1280-q4-generated.cpp. Q15.6/Q15.7 subsequently patch that same file. This
# layer therefore edits the FINAL generated OpenXR source in place.

set(Q1600_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1600_Q4_INPUT}")
    message(FATAL_ERROR "Q15.11 expected final OpenXR source at ${Q1600_Q4_INPUT}")
endif()
file(READ "${Q1600_Q4_INPUT}" Q1600_Q4_SOURCE)

set(Q1600_GEOMETRY_OLD [=[
        vertices.insert(vertices.end(), controllerVertices.begin(), controllerVertices.end());
        controllerVertexCount_ = 6;

        glGenVertexArrays(1, &vao_);
]=])
set(Q1600_GEOMETRY_NEW [=[
        vertices.insert(vertices.end(), controllerVertices.begin(), controllerVertices.end());
        controllerVertexCount_ = 6;

        // Q15.11: vector text "Q15.11" in left-controller local space.
        // +Y places it physically above the hand; +Z keeps it just in front of
        // the controller body. Lines avoid any font/texture dependency.
        versionStartVertex_ = static_cast<GLint>(vertices.size() / 3);
        auto q1600Line = [&](float x0, float y0, float x1, float y1) {
            constexpr float z = 0.035f;
            vertices.insert(vertices.end(), {x0, y0, z, x1, y1, z});
        };
        constexpr float q1600Y = 0.072f;
        constexpr float q1600W = 0.022f;
        constexpr float q1600H = 0.040f;
        constexpr float q1600Gap = 0.007f;
        float q1600X = -0.078f;

        // Q
        q1600Line(q1600X, q1600Y, q1600X + q1600W, q1600Y);
        q1600Line(q1600X, q1600Y + q1600H, q1600X + q1600W, q1600Y + q1600H);
        q1600Line(q1600X, q1600Y, q1600X, q1600Y + q1600H);
        q1600Line(q1600X + q1600W, q1600Y, q1600X + q1600W, q1600Y + q1600H);
        q1600Line(q1600X + q1600W * 0.52f, q1600Y + q1600H * 0.20f,
                  q1600X + q1600W * 1.15f, q1600Y - 0.006f);
        q1600X += q1600W + q1600Gap;

        // Seven-segment helper: A B C D E F G -> bits 0..6.
        auto q1600Digit = [&](float x, unsigned mask) {
            const float y = q1600Y;
            const float m = y + q1600H * 0.5f;
            const float t = y + q1600H;
            if (mask & (1u << 0)) q1600Line(x, t, x + q1600W, t);
            if (mask & (1u << 1)) q1600Line(x + q1600W, t, x + q1600W, m);
            if (mask & (1u << 2)) q1600Line(x + q1600W, m, x + q1600W, y);
            if (mask & (1u << 3)) q1600Line(x, y, x + q1600W, y);
            if (mask & (1u << 4)) q1600Line(x, m, x, y);
            if (mask & (1u << 5)) q1600Line(x, t, x, m);
            if (mask & (1u << 6)) q1600Line(x, m, x + q1600W, m);
        };
        constexpr unsigned q1600Digit1 = 0x06u; // B C
        constexpr unsigned q1600Digit5 = 0x6Du; // A F G C D

        q1600Digit(q1600X, q1600Digit1);
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, q1600Digit5);
        q1600X += q1600W + q1600Gap;

        // Decimal point.
        q1600Line(q1600X + 0.002f, q1600Y,
                  q1600X + 0.002f, q1600Y + 0.0045f);
        q1600X += 0.006f + q1600Gap;

        q1600Digit(q1600X, q1600Digit1);
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, q1600Digit1);

        versionVertexCount_ =
            static_cast<GLsizei>(vertices.size() / 3 - versionStartVertex_);
        FQ_LOGI("Q15.11 BUILD LABEL: text=Q15.11 anchor=left-hand vertices=%d alwaysVisible=1",
                static_cast<int>(versionVertexCount_));

        glGenVertexArrays(1, &vao_);
]=])
string(FIND "${Q1600_Q4_SOURCE}" "${Q1600_GEOMETRY_OLD}" Q1600_GEOMETRY_POS)
if(Q1600_GEOMETRY_POS EQUAL -1)
    message(FATAL_ERROR "Q15.11 could not find controller geometry anchor")
endif()
string(REPLACE "${Q1600_GEOMETRY_OLD}" "${Q1600_GEOMETRY_NEW}"
       Q1600_Q4_SOURCE "${Q1600_Q4_SOURCE}")

set(Q1600_HAND_DRAW_OLD [=[
                if (hand == 0) SetMvpAndColor(mvp, 0.25f, 0.68f, 1.0f);
                else SetMvpAndColor(mvp, 1.0f, 0.48f, 0.18f);
                glDrawArrays(GL_TRIANGLES, controllerStartVertex_, controllerVertexCount_);
]=])
set(Q1600_HAND_DRAW_NEW [=[
                if (hand == 0) {
                    SetMvpAndColor(mvp, 0.25f, 0.68f, 1.0f);
                    glDrawArrays(GL_TRIANGLES, controllerStartVertex_, controllerVertexCount_);

                    // A bright floating build ID above the left hand. Force the
                    // label through depth only for these line vertices, then put
                    // the normal world/controller depth function straight back.
                    glDepthFunc(GL_ALWAYS);
                    glLineWidth(2.0f);
                    SetMvpAndColor(mvp, 1.0f, 0.86f, 0.10f);
                    glDrawArrays(GL_LINES, versionStartVertex_, versionVertexCount_);
                    glDepthFunc(GL_LEQUAL);
                } else {
                    SetMvpAndColor(mvp, 1.0f, 0.48f, 0.18f);
                    glDrawArrays(GL_TRIANGLES, controllerStartVertex_, controllerVertexCount_);
                }
]=])
string(FIND "${Q1600_Q4_SOURCE}" "${Q1600_HAND_DRAW_OLD}" Q1600_HAND_DRAW_POS)
if(Q1600_HAND_DRAW_POS EQUAL -1)
    message(FATAL_ERROR "Q15.11 could not find left/right controller draw anchor")
endif()
string(REPLACE "${Q1600_HAND_DRAW_OLD}" "${Q1600_HAND_DRAW_NEW}"
       Q1600_Q4_SOURCE "${Q1600_Q4_SOURCE}")

set(Q1600_MEMBER_OLD [=[
    GLint controllerStartVertex_{0};
    GLsizei controllerVertexCount_{0};
    uint64_t frameCounter_{0};
]=])
set(Q1600_MEMBER_NEW [=[
    GLint controllerStartVertex_{0};
    GLsizei controllerVertexCount_{0};
    GLint versionStartVertex_{0};
    GLsizei versionVertexCount_{0};
    uint64_t frameCounter_{0};
]=])
string(FIND "${Q1600_Q4_SOURCE}" "${Q1600_MEMBER_OLD}" Q1600_MEMBER_POS)
if(Q1600_MEMBER_POS EQUAL -1)
    message(FATAL_ERROR "Q15.11 could not find controller member anchor")
endif()
string(REPLACE "${Q1600_MEMBER_OLD}" "${Q1600_MEMBER_NEW}"
       Q1600_Q4_SOURCE "${Q1600_Q4_SOURCE}")

file(WRITE "${Q1600_Q4_INPUT}" "${Q1600_Q4_SOURCE}")

string(FIND "${Q1600_Q4_SOURCE}" "text=Q15.11 anchor=left-hand" Q1600_LABEL_OK)
string(FIND "${Q1600_Q4_SOURCE}" "glDrawArrays(GL_LINES, versionStartVertex_, versionVertexCount_)" Q1600_DRAW_OK)
string(FIND "${Q1600_Q4_SOURCE}" "GLint versionStartVertex_{0};" Q1600_MEMBER_OK)
if(Q1600_LABEL_OK EQUAL -1 OR Q1600_DRAW_OK EQUAL -1 OR Q1600_MEMBER_OK EQUAL -1)
    message(FATAL_ERROR
        "Q15.11 build-label verification failed: label=${Q1600_LABEL_OK} draw=${Q1600_DRAW_OK} member=${Q1600_MEMBER_OK}")
endif()

message(STATUS "Q15.11 floating left-hand build label enabled: Q15.11")
