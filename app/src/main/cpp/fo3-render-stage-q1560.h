#pragma once

// Q15.6 renderer-stage isolator shared by the native renderer and OpenXR input.
//
// The app launches in FINAL so Q15.6 is visually identical to the clean Q15.5
// authored-lighting baseline until LEFT X is pressed. Each rising edge cycles:
//
//   FINAL -> RAW -> FOG -> HDR -> FINAL
//
// RAW   = authored world/material lighting, no WTHR fog, no fullscreen post.
// FOG   = RAW + authored WTHR fog, no fullscreen post.
// HDR   = FOG + current SP17 HDR/adaptation path, cinematic IMGS flags disabled.
// FINAL = normal current renderer (FOG + HDR + cinematic/ImageSpace).
//
// LEFT Y remains the Q15.5 no-op authored-lighting state. The obsolete Q15.4
// tangent-light selector is not toggled by Q15.6 and therefore stays on its
// default Quest-world-normal side for every stage.

enum Fo3RenderStageQ1560 : int {
    FO3_RENDER_STAGE_RAW_Q1560 = 0,
    FO3_RENDER_STAGE_FOG_Q1560 = 1,
    FO3_RENDER_STAGE_HDR_Q1560 = 2,
    FO3_RENDER_STAGE_FINAL_Q1560 = 3,
};

inline int gFo3RenderStageQ1560 = FO3_RENDER_STAGE_FINAL_Q1560;

inline int GetFo3RenderStageQ1560() {
    return gFo3RenderStageQ1560;
}

inline void CycleFo3RenderStageQ1560() {
    gFo3RenderStageQ1560 = (gFo3RenderStageQ1560 + 1) & 3;
}

inline const char* GetFo3RenderStageNameQ1560() {
    switch (gFo3RenderStageQ1560) {
        case FO3_RENDER_STAGE_RAW_Q1560: return "RAW_LIGHTING";
        case FO3_RENDER_STAGE_FOG_Q1560: return "PLUS_FOG";
        case FO3_RENDER_STAGE_HDR_Q1560: return "PLUS_HDR_NO_CINEMATIC";
        default: return "FINAL";
    }
}
