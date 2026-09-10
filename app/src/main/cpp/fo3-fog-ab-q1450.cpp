#include "fo3-fog-ab-q1450.h"
#include "fo3-render-stage-q1560.h"

// LAND is built as a separate translation unit. Keep the shared inline state in
// the headers and expose ordinary external symbols for the generated terrain
// renderer to query.
bool GetFo3FogEnabledQ1450Bridge() {
    return GetFo3FogEnabledQ1450();
}

int GetFo3RenderStageQ1560Bridge() {
    return GetFo3RenderStageQ1560();
}
