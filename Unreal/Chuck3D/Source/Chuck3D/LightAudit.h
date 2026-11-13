#pragma once

#include "CoreMinimal.h"

class UWorld;

// Unshadowed lights against strand hair (2026-11-13). With r.HairStrands.Voxelization off (Waterdeep's frame
// rate) strands have no self-shadow, so a light that casts no shadows lights hair through the head: dark hair
// reads grey, red reads pale, purple light makes beards blaze (the heroes, Jeffries and the crew, Chuck's fur:
// three fixes on 2026-11-09 alone). This lists every visible point/spot light with shadows off whose reach
// covers a groom, as CHUCK_LIGHT_AUDIT lines and a summary CHUCK_LIGHT_AUDIT_DONE risky=<n>.
//
// When it flags one: give the light shadows (SetCastShadows(true)), or move or shrink it off the hair, or turn
// voxels on near that hair (ChuckGroom::Want, as the temple heroes do). Run it from a shot list ("audit").
namespace ChuckLightAudit
{
    int32 Run(UWorld* World, const TCHAR* Where);
}
