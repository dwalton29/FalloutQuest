#pragma once

#include "fo3-nif-mesh.h"

// Loads the visible chair03.nif NiTriStripsData from Fallout - Meshes.bsa.
// Positions remain in native Fallout/Gamebryo units and indices are expanded
// into an ordinary triangle list suitable for GLES rendering.
bool LoadMegatonChairStripMesh(Fo3StaticMesh& outMesh);
