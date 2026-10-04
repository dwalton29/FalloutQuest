#pragma once
#include "fo3-interaction-hud-renderer.h"
// Compatibility entry point. There is one font layout and one prompt renderer.
inline void RenderFo3InteractionHud(const float* mvp,const char* prompt) {
    fo3hudrenderer::Render(mvp,prompt);
}
