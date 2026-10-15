#pragma once
#include "CoreMinimal.h"

class AActor;
class AChuckCharacter;

// The gorge the rope bridge crosses, and the land on both rims (Claude,
// 2026-10-15; docs/CHULT-GORGE.md). Canyon frame (ChultCanyonFrame): S runs
// across the gorge, T along it. Side 0 is the starting side, its wall facing
// +S; side 1 the far side. Inside the crossing (|T|<=4500 near, 6500 far) the
// lips are exactly the existing banks' edges, S=0 and S=4500.
float ChultGorgeLip(int32 Side,float T);
float ChultGorgeTop(int32 Side,float T);
// Open land beyond the existing floors (equal to them inside). Side -1 picks
// the nearer rim.
float ChultGorgeLandZ(float S,float T,int32 Side=-1);
// The wall face at height Z (frame-local), below that side's top.
float ChultGorgeWallS(int32 Side,float T,float Z);
// 0..1: how much of a weathered, vegetated bench the wall is at that height.
float ChultGorgeBench(int32 Side,float T,float Z);
// Distance from the existing floors (<=0 inside one).
float ChultGorgeFloorDistance(float S,float T);
// Columns along the gorge shared by both walls and both rims, so they meet exactly.
TArray<float> ChultGorgeColumns();
// Free-falling water: 0 the creek outfall, 1 the broad river fall, then the
// hanging valleys' falls. Drop is measured down from the lip, Across in -.5..5.
int32 ChultGorgeFallCount();
int32 ChultGorgeFallSide(int32 Fall);
float ChultGorgeFallDrop(int32 Fall);
FVector ChultGorgeFall(int32 Fall,float Drop,float Across);
// Below this the gorge is a sea of mist.
constexpr float ChultGorgeMistZ=-5200.f;

void BuildChultGorge(AActor* Owner);
int32 CheckChultGorge(AActor* Owner,AChuckCharacter* Chuck);
