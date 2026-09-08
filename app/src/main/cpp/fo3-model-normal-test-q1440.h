#pragma once

// Q14.4 diagnostic state shared by the OpenXR input host and static/NIF renderer.
// false = existing mapped-normal directional diffuse path (Q14.3 baseline)
// true  = authored NIF vertex normal for directional diffuse only.
//
// Normal maps remain loaded and continue to drive specular/local-light work in
// both modes. This switch deliberately changes only the normal used for the
// exterior directional-sun Lambert term.
inline bool& Fo3UseVertexNormalForSunQ1440State() {
    static bool useVertexNormal = false;
    return useVertexNormal;
}

inline bool GetFo3UseVertexNormalForSunQ1440() {
    return Fo3UseVertexNormalForSunQ1440State();
}

inline void ToggleFo3UseVertexNormalForSunQ1440() {
    Fo3UseVertexNormalForSunQ1440State() = !Fo3UseVertexNormalForSunQ1440State();
}

inline const char* GetFo3ModelNormalModeNameQ1440() {
    return GetFo3UseVertexNormalForSunQ1440() ? "NIF_VERTEX_NORMAL" : "MAPPED_NORMAL";
}
