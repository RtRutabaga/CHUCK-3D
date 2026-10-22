#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;

// The temple interior (Claude, 2026-10-20; docs/CHULT-TEMPLE-INTERIOR.md),
// after the 2D game's Phase 6 dungeon: through the sanctuary's doorway, a
// hidden stair inside the masonry, a great well sunk under the temple with a
// stair round its walls, then under the forecourt the Great Hall, the spike
// corridor, the pier hall with its Astral rift and the dart gallery.
// Coordinates are local to Chult's FloorOrigin; S and T the canyon frame, Z
// above the temple plaza.

// The spaces a death returns him to the start of (each has a Dev Checkpoint).
// Since October 22 the trap run's sections too (ChultTempleRun.h): the rubble's foot, the pits,
// the axes, the fire, the saws and the gauntlet.
enum class ETempleInteriorSpawn : uint8 { Well, Hall, Spikes, Piers, Darts, Snakes, Climb, Pits, Axes, Fire, Saws, Gauntlet };
constexpr int32 TempleInteriorSpawnCount = 12;
FVector ChultTempleInteriorSpawn(ETempleInteriorSpawn Spawn);   // world, capsule centre
float ChultTempleInteriorSpawnYaw(ETempleInteriorSpawn Spawn);
const TCHAR* ChultTempleInteriorSpawnName(ETempleInteriorSpawn Spawn);
// After a death inside: the start of the space he died in. False elsewhere.
bool ChultTempleInteriorRespawn(const FVector& DeathWorld, FVector& OutWorld);
bool IsChultTempleInteriorSpawn(const FVector& World, float& OutYaw);

// Inside the interior (world), Margin widening every space: the light and fog
// change while the camera is in, and the bird swoops keep out.
bool InChultTempleInterior(const FVector& World, float Margin = 0);
// Below the walkway into the well's pit or the pier hall's rift: an Astral death.
bool ChultTempleInteriorSwallows(const FVector& FeetWorld);

// For the exterior's builder: the parts of the horizontal plane Z (above the
// plaza) the interior passes through, as frame rectangles (S in X, T in Y);
// the masonry's faces are left open there.
void ChultTempleInteriorCutsAt(float Z, TArray<FBox2D>& Out);
// For the precinct floor: no ground over the well (frame S,T of a cell's centre).
bool ChultTempleInteriorCutsGround(float S, float T);

// The temple skeletons' posts (Claude, 2026-10-21; docs/CHULT-TEMPLE-SKELETONS.md):
// five in the spike corridor (one to each gap between the bands after the first,
// kept to it), twelve in the pier hall (seven before the rift, five beyond it,
// each kept to its side). Leash: the frame rectangle (S in X, T in Y) its body
// keeps inside: never into a spike band, the rift or another space.
struct FTempleSkeletonPost
{
    FVector Feet = FVector::ZeroVector;   // world
    float Yaw = 0;
    bool bDormant = false;                // lying among the bones until he comes near
    FBox2D Leash = FBox2D(ForceInit);
    const TCHAR* Space = TEXT("");
};
void ChultTempleSkeletonPosts(TArray<FTempleSkeletonPost>& Out);
// The snake chamber's twenty snakes (October 21; docs/CHULT-TEMPLE-SNAKES.md): each lies coiled at its
// post in the lanes round the piers; some wander their lane. All keep to the chamber (Leash, frame S/T).
struct FTempleSnakePost
{
    FVector Feet = FVector::ZeroVector;   // world, the coil's centre on the floor
    float Yaw = 0;
    bool bWander = false;
    FBox2D Leash = FBox2D(ForceInit);
};
void ChultTempleSnakePosts(TArray<FTempleSnakePost>& Out);
// Frame (S,T) rectangles of the chamber's piers, idol plinths and brazier (the snakes steer round them).
void ChultTempleSnakeObstacles(TArray<FBox2D>& Out);
// World position to the canyon frame (S, T, Z above the plaza), and its floor height (world Z) in the halls.
FVector ChultTempleInteriorFrameOf(const FVector& World);
FVector ChultTempleInteriorWorldOf(const FVector& Frame);
float ChultTempleInteriorFloorZ();

void BuildChultTempleInterior(AActor* Owner);
int32 CheckChultTempleInterior(AActor* Owner, AChuckCharacter* Chuck);
// -ChuckChultTest -ChuckTempleInteriorReview: cooked views, then the real
// character walks down from the sanctuary to the Great Hall and through each trap.
void ReviewChultTempleInterior(AActor* Owner, AChuckCharacter* Chuck, int32 Failures);
