#pragma once
#include "CoreMinimal.h"

/**
 * Something Chuck climbs hand over hand, up and down its whole height, without
 * jumping onto it (user 2026-10-03: the pantry ladder first; ropes and the like
 * later use the same). He takes hold by walking into its foot, or by walking
 * toward its top lip from the floor above (he turns and lowers himself onto
 * it); he pulls himself up over the lip at the top and steps off at the foot.
 * The world builders register them; AChuckCharacter's Ladder gait climbs them.
 */
struct FChuckClimbable
{
    FVector Foot = FVector::ZeroVector;     // the climbing line at floor height: where his centre goes, z of the floor
    float TopZ = 0.f;                        // height of the lip he climbs out over
    FVector Out = FVector::ForwardVector;    // horizontal, from the ladder toward the climber (he faces -Out)
    float HalfWidth = 25.f;                  // how far to either side of the line he can take hold
    FVector Lip = FVector::ZeroVector;       // a point on the top edge (its face looks along Out); the floor above is on its -Out side
};

/** Register one (a world builder does this; the same one twice is ignored). */
void AddChuckClimbable(const FChuckClimbable& Climbable);
const TArray<FChuckClimbable>& GetChuckClimbables();
