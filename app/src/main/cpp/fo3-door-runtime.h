#pragma once

// Called from the OpenXR input loop on a right-trigger press. The origin and
// direction are already in FalloutQuest virtual/world metres. Returns true only
// when an authored Fallout load-door transition completed successfully.
bool ActivateFo3DoorQ7(float originX, float originY, float originZ,
                       float dirX, float dirY, float dirZ);
