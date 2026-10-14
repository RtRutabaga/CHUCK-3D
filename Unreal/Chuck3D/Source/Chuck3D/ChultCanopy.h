#pragma once
#include "CoreMinimal.h"

class AActor;
class AChuckCharacter;
// Setting coordinates are centimetres relative to AChultLanding::FloorOrigin().
// Four single-trunk canopy giants (Claude, 2026-10-13, replacing the paired
// climbing trunks): each has a crotch to stand in, a broad walking limb toward
// each neighbour and, above it, a thinner hand bough whose underside carries
// the brachiation grips (registered with AddChuckSwingGrip). A liana hangs
// from one limb of each tree to the ground: a climbable (AddChuckClimbable).
struct FChultCanopyChain
{
    FName Name;
    TArray<FVector> Paws;       // grip points, in order from tree From to tree To
    bool bVine = false;
    // First paw on the neighbouring tree: no wood spans this transition.
    int32 BranchBreak = INDEX_NONE;
    int32 From = 0, To = 0;
};
struct FChultCanopyTree
{
    FVector Trunk;              // trunk axis at the ground
    float Top = 0;              // crotch and walking-limb top height
    FVector Fork;               // a standing point in the crotch (z: the bark top)
    TArray<FVector> Limbs;      // horizontal walking-limb directions (next tree, previous tree, extras)
    FVector LianaFoot, LianaOut, LianaLip;
};
const TArray<FChultCanopyChain>& ChultCanopyChains();
const TArray<FChultCanopyTree>& ChultCanopyTrees();
// Top of walking limb Limb of tree Tree, S cm out from the trunk axis (local).
FVector ChultCanopyLimbTop(int32 Tree, int32 Limb, float S);
bool InChultCanopyReserve(const FVector& LocalPosition, float Margin = 0);
void BuildChultCanopy(AActor* Owner);
// Static traversal checks (grip clearance, leap sweeps, standing limbs, lianas).
int32 CheckChultCanopy(AActor* Owner, AChuckCharacter* Chuck);
// -ChuckCanopyTest: the real character climbs a liana, brachiates the whole loop
// tree to tree, reverses, lets go and climbs back down; then quits.
void StartChultCanopyReview(AActor* Owner, AChuckCharacter* Chuck);
