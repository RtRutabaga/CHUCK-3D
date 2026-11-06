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
    bool bBlocked = false;                   // closed for now (the ship's storm rupture over the companion, 2026-11-04): not taken hold of
    // A net, slanted or bent (the ship's ratlines, 2026-11-05). Line: where his centre goes, bottom to top (world; each
    // point's Z his centre's height there; the first over Foot); empty, straight up from Foot as a ladder. SideTravel: how
    // far either side of the line he can climb across, at the bottom and at the top (a ladder or rope: 0). Approach: the
    // way he walks to take hold at the foot, when it isn't toward it (-Out): from the deck out over the rail onto the
    // shrouds' outer face, turning round at TurnZ (his centre's height) to face -Out. bTopOut false: no lip at the top,
    // the climb just ends there (the topmast shrouds under the crosstrees).
    TArray<FVector> Line;
    float SideTravel = 0.f, TopSideTravel = 0.f;
    FVector Approach = FVector::ZeroVector;
    float TurnZ = 0.f;
    bool bTopOut = true;
};

/** Along its line at his centre's height Z (clamped to its ends; a ladder: over Foot). */
FVector ChuckClimbPoint(const FChuckClimbable& C, float Z);
/** How far either side of its line he can be at height Z (Bottom and Top: the ends of his climb). */
float ChuckClimbSideTravel(const FChuckClimbable& C, float Z, float Bottom, float Top);
/** The way he faces on it at height Z (horizontal; -Out except where he turns round from Approach). */
FVector ChuckClimbFacing(const FChuckClimbable& C, float Z);
/** The way the line runs up at height Z (unit; straight up for a ladder). */
FVector ChuckClimbUp(const FChuckClimbable& C, float Z);

/** Register one (a world builder does this; the same one twice is ignored). */
void AddChuckClimbable(const FChuckClimbable& Climbable);
const TArray<FChuckClimbable>& GetChuckClimbables();
/** Close (or reopen) the climbable whose top lip is at Lip (within 5 cm). False if there is none. */
bool SetChuckClimbableBlocked(const FVector& Lip, bool bBlocked);

/**
 * Something he swings from by both paws, hand over hand to the next (user
 * 2026-10-09: brachiation; the high wall lanterns' iron rings first, Chult's
 * branches later). Caught in the air near it; AChuckCharacter's Swing gait.
 */
struct FChuckSwingGrip
{
    FVector Grip = FVector::ZeroVector;      // the bar his paws close round
    FVector Along = FVector::ForwardVector;  // horizontal: the line he swings along (either way)
    // A board's width along that line (Chult's bridge planks, 2026-10-31), or 0
    // for a bar or ring: he holds a board by its near edge, his head clear of
    // it in the gap he came from, and flies board to board low under the deck.
    float Depth = 0.f;
};

void AddChuckSwingGrip(const FChuckSwingGrip& Grip);
const TArray<FChuckSwingGrip>& GetChuckSwingGrips();
