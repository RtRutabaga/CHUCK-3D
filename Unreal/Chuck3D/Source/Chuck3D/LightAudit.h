#pragma once

#include "CoreMinimal.h"

class UWorld;

// Unshadowed lights against strand hair (2026-11-13). With r.HairStrands.Voxelization off (Waterdeep's frame
// rate) strands have no self-shadow, so a light that casts no shadows lights hair through the head: dark hair
// reads grey, red reads pale, purple light makes beards blaze (the heroes, Jeffries and the crew, Chuck's fur:
// three fixes on 2026-11-09 alone). This lists every visible point/spot light with shadows off whose reach
// covers a groom brightly enough to matter, as CHUCK_LIGHT_AUDIT lines and a summary CHUCK_LIGHT_AUDIT_DONE risky=<n>.
//
// It is a list of candidates to look at, not a pass/fail (brightness alone does not separate the reported cases
// from Waterdeep's lamps, which were never reported; see LightAudit.cpp). When one reads badly in a still: give the light shadows (SetCastShadows(true)), or move or shrink it off the hair, or turn
// voxels on near that hair (ChuckGroom::Want, as the temple heroes do). Run it from a shot list ("audit").
namespace ChuckLightAudit
{
    /** Threshold: the estimated light on the hair (lux-like units) worth flagging. */
    int32 Run(UWorld* World, const TCHAR* Where, float Threshold = 100.f);
}
