#include "fo3-fog-ab-q1450.h"

// LAND is built as a separate translation unit. Keep the Q14.5 toggle state in
// the inline header variable and expose one ordinary external symbol for the
// generated terrain renderer to query.
bool GetFo3FogEnabledQ1450Bridge() {
    return GetFo3FogEnabledQ1450();
}
