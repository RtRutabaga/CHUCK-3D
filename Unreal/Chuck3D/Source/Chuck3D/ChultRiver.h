#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;
// The far bank's floor runs from the gorge (S=4500) to ChultFarFloorEnd
// (extended upstream by Claude, 2026-10-15: docs/CHULT-UPRIVER.md); the river
// rises at a pool under the source falls at S=ChultRiverHead.
// Since the temple (2026-10-15, docs/CHULT-TEMPLE.md) it runs on to S=92000.
constexpr float ChultFarFloorEnd=92000.f;
constexpr float ChultRiverHead=33800.f;
bool InChultRiverExtension(const FVector& P);
float ChultRiverCenter(float S);
float ChultRiverWidth(float S);
float ChultRiverWaterZ(float S);
float ChultRiverDistance(const FVector& P);
float ChultRiverGroundZ(const FVector& P,float Base);
void BuildChultRiver(AActor* Owner);
int32 CheckChultRiver(AActor* Owner,AChuckCharacter* Chuck);
void StartChultRiverWalks(AActor* Owner,AChuckCharacter* Chuck);
