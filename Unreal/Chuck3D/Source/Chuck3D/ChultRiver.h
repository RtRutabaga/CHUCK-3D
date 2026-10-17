#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;
// The far bank's floor runs from the gorge (S=4500) to ChultFarFloorEnd
// (extended upstream by Claude, 2026-10-15: docs/CHULT-UPRIVER.md); the river
// rises at a pool under the source falls at S=ChultRiverHead.
// Since the temple (2026-10-15, docs/CHULT-TEMPLE.md) it runs on to S=92000.
constexpr float ChultFarFloorEnd=92000.f;
// The temple precinct (2026-10-16): from S=62000 to the floor's end the floor
// widens to |T|<=10500 for the mega temple's twin pyramids and the jungle round them.
constexpr float ChultTemplePrecinctFrom=62000.f,ChultTemplePrecinctHalf=10500.f;
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
