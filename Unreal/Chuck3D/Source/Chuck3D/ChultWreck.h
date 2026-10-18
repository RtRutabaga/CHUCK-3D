#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;

// The jungle wreck (Claude, 2026-10-18; docs/CHULT-WRECK.md): a cog wrecked
// not long ago, lying heeled over just beyond the bridge, beside the far road
// that every crossing takes. No crew. Astral ruptures, the sewer's natural
// kind, open in the ground round it. Coordinates are local to Chult's
// FloorOrigin; S and T are the canyon frame (ChultCanyonFrame).

// True when the far bank's 1 m ground cell whose low corner is (S,T) is cut
// by the wreck (the hull's buried part, or a rupture). ChultCanyon leaves such
// cells out and BuildChultWreck lays them again with the cut taken out.
bool ChultWreckCutsCell(float S,float T);
// His feet (world) have gone down into a rupture: an Astral death.
bool ChultWreckRuptureSwallows(const FVector& FeetWorld);

void BuildChultWreck(AActor* Owner);
int32 CheckChultWreck(AActor* Owner,AChuckCharacter* Chuck);
// -ChuckChultTest -ChuckWreckReview: cooked views, then the real character
// walks the road past it, drops in through the breach, climbs the hold ramp
// onto the deck and leaves by the fallen bulwark, walks into the cabin and
// into a rupture; then quits.
void ReviewChultWreck(AActor* Owner,AChuckCharacter* Chuck,int32 Failures);
