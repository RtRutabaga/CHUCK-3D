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
    int32 LimbA = 0, LimbB = 1;   // the walking limb each end grip hangs over (index into that tree's Limbs)
};
struct FChultCanopyTree
{
    FVector Trunk;              // trunk axis at the ground
    float Top = 0;              // crotch and walking-limb top height
    float TrunkScale = 1;       // girth (natural variety between the trees)
    FVector Fork;               // a standing point in the crotch (z: the bark top)
    // Walking limbs: next tree, previous tree, (tree 0) the ruin slab, then spurs.
    TArray<FVector> Limbs;      // horizontal directions
    TArray<float> LimbLength, LimbRise, LimbScale, LimbAway;   // walking length, top rise per cm, girth, side its tip bends to
    FVector LianaFoot, LianaOut, LianaLip;
    bool bLiana = true;         // the giants upriver: only the last one has a liana
};
// A side branch (2026-10-13 user follow-up: branches in all directions, all
// brachiatable): a walking limb out of the crotch with its own hand bough and
// grips, a dead end - out along it and back, stepping down onto the limb.
struct FChultCanopySpur
{
    FName Name;
    int32 Tree = 0, Limb = 0;
    TArray<FVector> Paws;       // from over the limb outward
};
const TArray<FChultCanopyChain>& ChultCanopyChains();
const TArray<FChultCanopySpur>& ChultCanopySpurs();
const TArray<FChultCanopyTree>& ChultCanopyTrees();
// Top of walking limb Limb of tree Tree, S cm out from the trunk axis (local).
FVector ChultCanopyLimbTop(int32 Tree, int32 Limb, float S);
bool InChultCanopyReserve(const FVector& LocalPosition, float Margin = 0);
// Where he comes back after dying (user 2026-10-14): anywhere around the
// brachiation trees, on the top of the ruin's landing slab that leads onto
// tree 0, facing it; elsewhere in Chult, the landing entrance. World space.
bool InChultCanopyDeathArea(const FVector& WorldPosition);
FVector ChultCanopyRespawn();
float ChultCanopyRespawnYaw();
void BuildChultCanopy(AActor* Owner);
// Static traversal checks (grip clearance, leap sweeps, standing limbs, lianas).
int32 CheckChultCanopy(AActor* Owner, AChuckCharacter* Chuck);
// -ChuckCanopyTest: the real character climbs a liana, brachiates the whole loop
// tree to tree, reverses, lets go and climbs back down; then quits.
void StartChultCanopyReview(AActor* Owner, AChuckCharacter* Chuck);
// The giants upriver (user 2026-10-15; docs/CHULT-UPRIVER.md): a second grove
// built by the same code, its crotches 15-17 m up (reached later with the
// golden cigarette's stamina lock), a line of chains along the T-rex zone and
// spurs, with a liana only on its last tree. Its trunks stand on the far
// bank's ground (FChultCanopyTree::Trunk.Z). The functions above stay grove 0.
void BuildChultGiantCanopy(AActor* Owner);
int32 CheckChultGiantCanopy(AActor* Owner, AChuckCharacter* Chuck);
// -ChuckGiantCanopyTest: from the first giant's crotch along every chain, down
// the last liana, deaths up there and below, every spur; then quits.
void StartChultGiantCanopyReview(AActor* Owner, AChuckCharacter* Chuck);
// The giants' trees, chains and spurs (local), for spawn points and views.
const TArray<FChultCanopyTree>& ChultGiantTrees();

FVector ChultGoldenLeafPosition();
FVector ChultGoldenClimbOut();
void StartChultGoldenLeafReview(AActor* Owner, AChuckCharacter* Chuck);
