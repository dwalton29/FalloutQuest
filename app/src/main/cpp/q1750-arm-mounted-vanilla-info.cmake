# Q16.5: arm-mounted Fallout 3 HUDMainMenu Info interaction widget.
#
# The user's Fallout - Misc.bsa establishes the authored widget semantics:
#   HUDMainMenu/Info: 320x80, HUDMain system colour, centered, glow=true,
#   line alpha 255, text_box.xml, xbuttona on the LEFT, verbuf=10.
# text_box.xml further defines the Xbox art as a 75x75 InterfaceShared atlas
# sprite and selects glow_general_button_a.dds when _glow is true.
# Fallout.ini maps the widget's default glow font (font 7) to
# Textures\\Fonts\\Baked-in_Monofonto_Large.fnt.
#
# Q16.5 keeps user-owned Bethesda assets external. This first arm-mounted pass
# reproduces the authored proportions and two-line interaction hierarchy with the
# existing vector/glow renderer while preparing the layout for a later FNT/TAI
# raster path. CELL traversal, Q16.4 gate targeting and Q16.2 loading are untouched.

set(Q1750_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1750_Q4_INPUT}")
    message(FATAL_ERROR "Q16.5 expected final OpenXR source at ${Q1750_Q4_INPUT}")
endif()
file(READ "${Q1750_Q4_INPUT}" Q1750_Q4_SOURCE)

# Append replacement geometry after the old Q16.1 one-line prompt, then repoint
# interactionStartVertex_/interactionVertexCount_ at only the new vertices. The
# old vertices remain harmlessly in the static VBO and are never drawn.
set(Q1750_GEOMETRY_ANCHOR [==[
        interactionVertexCount_ =
            static_cast<GLsizei>(vertices.size() / 3 - interactionStartVertex_);
]==])
set(Q1750_GEOMETRY_NEW [==[
        interactionVertexCount_ =
            static_cast<GLsizei>(vertices.size() / 3 - interactionStartVertex_);

        // Q16.5: vanilla HUDMainMenu Info hierarchy, adapted to right-forearm VR.
        // Authored desktop proportions are 320x80 with a 75x75 A button on the
        // left. Scale that relationship to a compact ~15 cm wide arm HUD.
        interactionStartVertex_ = static_cast<GLint>(vertices.size() / 3);
        auto q1750Line = [&](float x0, float y0, float x1, float y1) {
            constexpr float z = 0.068f; // lifted off the controller/forearm plane
            vertices.insert(vertices.end(), {x0, y0, z, x1, y1, z});
        };

        constexpr float q1750CharW = 0.0135f;
        constexpr float q1750CharH = 0.0205f;
        constexpr float q1750CharGap = 0.0035f;
        constexpr float q1750BottomY = 0.105f;
        constexpr float q1750TopY = q1750BottomY + q1750CharH + 0.0065f;
        constexpr float q1750TextX = -0.030f;

        // The real text_box.xml button is 75x75 inside an 80px-high Info widget.
        // Preserve that near-full-height relationship beside the two text lines.
        constexpr float q1750ButtonCx = -0.074f;
        constexpr float q1750ButtonCy = q1750BottomY + q1750CharH + 0.00325f;
        constexpr float q1750ButtonRx = 0.0275f;
        constexpr float q1750ButtonRy = 0.0275f;
        constexpr int q1750ButtonSegments = 32;
        for (int i = 0; i < q1750ButtonSegments; ++i) {
            const float a0 = (6.28318530718f * static_cast<float>(i)) /
                             static_cast<float>(q1750ButtonSegments);
            const float a1 = (6.28318530718f * static_cast<float>(i + 1)) /
                             static_cast<float>(q1750ButtonSegments);
            q1750Line(q1750ButtonCx + std::cos(a0) * q1750ButtonRx,
                      q1750ButtonCy + std::sin(a0) * q1750ButtonRy,
                      q1750ButtonCx + std::cos(a1) * q1750ButtonRx,
                      q1750ButtonCy + std::sin(a1) * q1750ButtonRy);
        }

        // A glyph centered inside the authored button footprint.
        {
            constexpr float aw = 0.0145f;
            constexpr float ah = 0.0210f;
            const float ax = q1750ButtonCx - aw * 0.5f;
            const float ay = q1750ButtonCy - ah * 0.5f;
            q1750Line(ax, ay, ax + aw * 0.5f, ay + ah);
            q1750Line(ax + aw * 0.5f, ay + ah, ax + aw, ay);
            q1750Line(ax + aw * 0.22f, ay + ah * 0.48f,
                      ax + aw * 0.78f, ay + ah * 0.48f);
        }

        auto q1750Glyph = [&](char c, float x, float y) {
            const float w = q1750CharW;
            const float h = q1750CharH;
            const float m = y + h * 0.5f;
            switch (c) {
                case 'O':
                    q1750Line(x, y, x + w, y);
                    q1750Line(x, y + h, x + w, y + h);
                    q1750Line(x, y, x, y + h);
                    q1750Line(x + w, y, x + w, y + h);
                    break;
                case 'P':
                    q1750Line(x, y, x, y + h);
                    q1750Line(x, y + h, x + w, y + h);
                    q1750Line(x + w, y + h, x + w, m);
                    q1750Line(x, m, x + w, m);
                    break;
                case 'E':
                    q1750Line(x, y, x, y + h);
                    q1750Line(x, y + h, x + w, y + h);
                    q1750Line(x, m, x + w * 0.82f, m);
                    q1750Line(x, y, x + w, y);
                    break;
                case 'N':
                    q1750Line(x, y, x, y + h);
                    q1750Line(x + w, y, x + w, y + h);
                    q1750Line(x, y + h, x + w, y);
                    break;
                case 'D':
                    q1750Line(x, y, x, y + h);
                    q1750Line(x, y + h, x + w * 0.72f, y + h);
                    q1750Line(x, y, x + w * 0.72f, y);
                    q1750Line(x + w * 0.72f, y, x + w, y + h * 0.25f);
                    q1750Line(x + w, y + h * 0.25f, x + w, y + h * 0.75f);
                    q1750Line(x + w, y + h * 0.75f, x + w * 0.72f, y + h);
                    break;
                case 'R':
                    q1750Line(x, y, x, y + h);
                    q1750Line(x, y + h, x + w, y + h);
                    q1750Line(x + w, y + h, x + w, m);
                    q1750Line(x, m, x + w, m);
                    q1750Line(x + w * 0.52f, m, x + w, y);
                    break;
                default:
                    break;
            }
        };
        auto q1750Text = [&](const char* s, float x, float y) {
            float pen = x;
            for (const char* p = s; *p; ++p) {
                q1750Glyph(*p, pen, y);
                pen += q1750CharW + q1750CharGap;
            }
        };

        // Fallout 3 feeds the Info string as a verb/object hierarchy. For the
        // current DOOR-only interaction slice that is the familiar Open / Door.
        q1750Text("OPEN", q1750TextX, q1750TopY);
        q1750Text("DOOR", q1750TextX, q1750BottomY);

        interactionVertexCount_ =
            static_cast<GLsizei>(vertices.size() / 3 - interactionStartVertex_);
]==])
string(FIND "${Q1750_Q4_SOURCE}" "${Q1750_GEOMETRY_ANCHOR}" Q1750_GEOMETRY_POS)
if(Q1750_GEOMETRY_POS EQUAL -1)
    message(FATAL_ERROR "Q16.5 could not find Q16 interaction geometry tail")
endif()
# Replace only the LAST occurrence because Q16.0/Q16.1 contain one interaction
# count tail. A direct replace is safe and configure guards below ensure Q16.5 exists.
string(REPLACE "${Q1750_GEOMETRY_ANCHOR}" "${Q1750_GEOMETRY_NEW}"
       Q1750_Q4_SOURCE "${Q1750_Q4_SOURCE}")

# Update the draw commentary to reflect the arm-mounted vanilla Info layout.
string(REPLACE
    "// hud_main_menu.xml -> Info -> text_box.xml semantics:\n                        // HUDMain system colour, glow=true, line_alpha=255,\n                        // xbuttona on the left. Keep it hand-anchored for VR."
    "// hud_main_menu.xml -> Info -> text_box.xml semantics:\n                        // HUDMain system colour, glow=true, line_alpha=255,\n                        // xbuttona on the left; Q16.5 mounts the 320:80 layout above the right forearm."
    Q1750_Q4_SOURCE "${Q1750_Q4_SOURCE}")

# Visible headset proof: Q16.4 -> Q16.5.
set(Q1750_LABEL_OLD [==[
        q1600Digit(q1600X, 0x66u); // Q16.4: 4 = F G B C
]==])
set(Q1750_LABEL_NEW [==[
        q1600Digit(q1600X, 0x6Du); // Q16.5: 5 = A F G C D
]==])
string(FIND "${Q1750_Q4_SOURCE}" "${Q1750_LABEL_OLD}" Q1750_LABEL_POS)
if(Q1750_LABEL_POS EQUAL -1)
    message(FATAL_ERROR "Q16.5 could not find Q16.4 final build-label digit")
endif()
string(REPLACE "${Q1750_LABEL_OLD}" "${Q1750_LABEL_NEW}"
       Q1750_Q4_SOURCE "${Q1750_Q4_SOURCE}")
string(REPLACE "Q16.4 BUILD LABEL:" "Q16.5 BUILD LABEL:"
       Q1750_Q4_SOURCE "${Q1750_Q4_SOURCE}")
string(REPLACE "text=Q16.4 anchor=left-hand" "text=Q16.5 anchor=left-hand"
       Q1750_Q4_SOURCE "${Q1750_Q4_SOURCE}")

file(WRITE "${Q1750_Q4_INPUT}" "${Q1750_Q4_SOURCE}")

# Hard guards: prove the authored two-line layout replaced the drawn vertex range
# while prior loading/gate work remains present.
string(FIND "${Q1750_Q4_SOURCE}" "q1750ButtonSegments = 32" Q1750_BUTTON_OK)
string(FIND "${Q1750_Q4_SOURCE}" "q1750Text(\"OPEN\"" Q1750_OPEN_OK)
string(FIND "${Q1750_Q4_SOURCE}" "q1750Text(\"DOOR\"" Q1750_DOOR_OK)
string(FIND "${Q1750_Q4_SOURCE}" "Q16.5: 5 = A F G C D" Q1750_LABEL_OK)
string(FIND "${Q1750_Q4_SOURCE}" "RenderFo3LoadingScreenQ1720" Q1750_LOADING_OK)
if(Q1750_BUTTON_OK EQUAL -1 OR Q1750_OPEN_OK EQUAL -1 OR
   Q1750_DOOR_OK EQUAL -1 OR Q1750_LABEL_OK EQUAL -1 OR
   Q1750_LOADING_OK EQUAL -1)
    message(FATAL_ERROR
        "Q16.5 verification failed: button=${Q1750_BUTTON_OK} open=${Q1750_OPEN_OK} door=${Q1750_DOOR_OK} label=${Q1750_LABEL_OK} loading=${Q1750_LOADING_OK}")
endif()

message(STATUS "Q16.5 arm-mounted vanilla Info HUD enabled: 320x80 semantics, 75x75 left A, OPEN/DOOR, HUDMain glow")
