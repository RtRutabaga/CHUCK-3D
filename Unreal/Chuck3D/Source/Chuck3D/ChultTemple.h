#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;

// The ancient temple's exterior (Claude, 2026-10-15; docs/CHULT-TEMPLE.md),
// after the 2D game's Chult map 5 (chult_temple.txt): a Mesoamerican stepped
// pyramid of four talud-tablero terraces, a broad stair walled in by
// balustrades that end in serpent heads at plaza level, a summit house with
// a dark doorway (the interior, Phase 6, is not built) and a roof comb, eight
// skull stakes lining the avenue to it, stelae and fallen carved blocks on a
// plaza framed by the deep jungle. It stands at the upriver valley's end.
// Coordinates are local to Chult's FloorOrigin; S and T the canyon frame.

// The plaza's level ground and the road's end at the stair's foot.
float ChultTempleGroundZ(const FVector& Local,float Base);
bool InChultTempleReserve(const FVector& Local,float Margin=0);
float ChultTempleRoadEndS();
// Standing at the head of the avenue facing the pyramid (world, capsule centre).
FVector ChultTempleSpawn();
float ChultTempleSpawnYaw();

// The temple's score (the user's "Chult 3", 2026-10-17): its swell, a full
// entry after a quiet build, comes ChultTempleMusicSwell seconds in. The zone
// starts that long a run (at RunSpeed) up the road before the temple comes
// into view, and covers the precinct; bWasInside widens it (hysteresis).
constexpr float ChultTempleMusicSwell=10.7f;
bool InChultTempleMusicZone(const FVector& World,bool bWasInside);
// Just inside the zone on the road: arrived by running up it, so play from the top.
bool ChultTempleMusicRunIn(const FVector& World);
// Where the temple comes into view on the road, and where its score starts (S).
float ChultTempleRevealS();
float ChultTempleMusicFromS();

// The central spire's tip and a pyramid summit house's roof (+T or -T side;
// local, above the plaza): what the birds circle and must clear (ChultBirds.h).
FVector ChultTempleSpireTop();
FVector ChultTemplePyramidTop(bool bPlusT);
float ChultTemplePlazaZ();

void BuildChultTemple(AActor* Owner);
int32 CheckChultTemple(AActor* Owner,AChuckCharacter* Chuck);
// -ChuckChultTest -ChuckTempleReview: cooked views, then the real character
// runs up the road past the reveal (the score's swell must meet it; with
// -ChuckTempleApproachShots a frame every 0.5 s), walks the avenue, climbs
// the stair and steps into the doorway; then quits.
void ReviewChultTemple(AActor* Owner,AChuckCharacter* Chuck,int32 Failures);
