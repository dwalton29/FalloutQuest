#pragma once

#include <cstdint>

// Q16.15 prepares the authored no-model/load-door XTEL anchors for one CELL.
// The cache is built from Fallout3.esm once when the scene becomes active; live
// controller ray misses never scan the ESM.
void PrimeFo3AuthoredDoorAnchorsQ1870(uint32_t cellFormId);
