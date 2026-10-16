#pragma once
#include "CoreMinimal.h"

class AActor;
class AChuckCharacter;
// The broken rope bridge over the northeast canyon (Claude, 2026-10-14; Codex
// built the canyon, ChultCanyon.h). A normal horizontal-plank rope bridge so
// decayed that he can only cross it hanging beneath: the few planks that still
// span from foot rope to foot rope are brachiation grips (AddChuckSwingGrip),
// everything between them is missing or broken off to nubs at the ropes. No
// plank or rope collides, so he never pulls himself up or stands on the deck.
// Coordinates: centimetres relative to AChultLanding::FloorOrigin(), in the
// canyon frame ChultCanyonFrame(S along, T across, Z).
struct FChultBridgePlank
{
    FName Name;
    FVector Grip;          // local: the plank's centre line, mid-thickness, where his paws close
    float S = 0;           // along the canyon frame
    bool bSplit = false;   // split lengthwise: a thin surviving half
};
// The surviving (grippable) planks, near bank to far bank.
const TArray<FChultBridgePlank>& ChultBridgePlanks();
// Top of the deck (where a plank's upper face lies) at S along the frame.
float ChultBridgeDeckZ(float S);
// Standing spots under the first and last plank (local, ground height).
FVector ChultBridgeNearStand();
FVector ChultBridgeFarStand();
void BuildChultBridge(AActor* Owner);
int32 CheckChultBridge(AActor* Owner, AChuckCharacter* Chuck);
// A fall into the gorge returns him to the bank he set out from (world space).
bool ChultBridgeRespawn(const FVector& DeathWorld, FVector& OutWorld);
bool IsChultBridgeRespawn(const FVector& World);
float ChultBridgeRespawnYaw();
// Those two gorge-return spots by the bridge (world), near or far bank: dev checkpoints too.
FVector ChultBridgeSpawn(bool bFar);
// The bridge and the far side have their own score (user 2026-10-14, "Chult 2").
// Hysteresis: once in, he leaves only well back on the near bank.
bool InChultBridgeMusicZone(const FVector& World, bool bWasInside);
// -ChuckBridgeTest: the real character crosses both ways, falls, and can't stand on it.
void StartChultBridgeReview(AActor* Owner, AChuckCharacter* Chuck);
