#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;

// Upriver beyond the bridge (Claude, 2026-10-15; docs/CHULT-UPRIVER.md): the far
// bank's floor carries on from S=28000 to ChultFarFloorEnd. The river rises at
// a spring-fed fall at its head; then the giant trees (a taller brachiation
// grove where a T-rex will later rule the ground); then dense jungle up to the
// space kept for the temple. Coordinates are local to Chult's FloorOrigin;
// S and T are the canyon frame (ChultCanyonFrame).

// Spawn points beyond the gorge. FarBank is the bridge's far-bank spot.
enum class EChultUpriverSpawn : uint8 { FarBank, Giants, GiantCanopy, DeepJungle, Temple };
// Standing location (world, capsule centre) and facing of a spawn point.
FVector ChultUpriverSpawn(EChultUpriverSpawn Spawn);
float ChultUpriverSpawnYaw(EChultUpriverSpawn Spawn);
// After a death on the far bank (not in the gorge): the spawn point of the
// area he died in. False elsewhere.
bool ChultUpriverRespawn(const FVector& DeathWorld,FVector& OutWorld);
// True (with its facing) when World is one of the far bank's ground spawns.
bool IsChultUpriverSpawn(const FVector& World,float& OutYaw);

// The old road's centre line upriver (T at S), for the road and its clearings.
float ChultUpriverRoadT(float S);
// The giants' winding line (T at S): trunk I stands 3.8 m either side of it at S=36600+1300*I.
float ChultGiantLineT(float S);
// The giant grove's trunk axes (local XY), its corridor, and the ground levelled under each trunk.
const TArray<FVector2D>& ChultGiantTrunkSpots();
bool InChultGiantReserve(const FVector& Local,float Margin=0);
// A screen of giants across the valley just past the T-rex's stretch (user
// 2026-10-19: hide the temple from the end of it). Scenery only: no grips.
const TArray<FVector2D>& ChultScreenTrunkSpots();
float ChultUpriverGroundZ(const FVector& Local,float Base);

void BuildChultUpriver(AActor* Owner);
int32 CheckChultUpriver(AActor* Owner,AChuckCharacter* Chuck);
// -ChuckChultTest -ChuckUpriverReview: cooked views, then quits.
void ReviewChultUpriver(AActor* Owner,AChuckCharacter* Chuck,int32 Failures);
