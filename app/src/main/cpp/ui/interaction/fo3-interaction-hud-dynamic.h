#pragma once
#include "fo3-interaction-hud-renderer.h"
// Legacy names delegate to the canonical renderer; no separate state/layout.
namespace fo3huddynamic = fo3hudrenderer;
inline void RenderFo3InteractionHudDynamic(const float* mvp,const char* prompt) {
    fo3hudrenderer::Render(mvp,prompt);
}
inline void ShutdownFo3InteractionHudDynamic() { fo3hudrenderer::Shutdown(); }
