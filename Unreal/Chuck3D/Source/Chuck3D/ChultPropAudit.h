#pragma once
#include "CoreMinimal.h"
class AActor;
class UWorld;
class AChuckCharacter;

// Props that sit wrongly in Chult (Claude, 2026-10-27; docs/CHULT-PROP-AUDIT.md).
//
// GroundChultTrees: the jungle's trees are planted at a ground height taken at their centre, so on a slope or a
// bank's edge part of the trunk's foot hangs over air. Once the world's collision is in, each tree is measured on a
// ring round its foot and let down onto the lowest ground there (its hidden trunk collision with it). A tree whose
// foot would have to drop more than MaxDrop is on a ledge and is left (and counted).
void GroundChultTrees(UWorld* World);
// The grounding is spread over frames; true once it has finished (or is switched off by -ChuckNoGrounding).
bool IsChultGroundingDone();

// -ChuckChultTest -ChuckPropAudit: measures what GroundChultTrees left, the fallen logs (closed and solid), and every
// jar and urn against the stone round it; stills of the worst; logs CHUCK_PROP_AUDIT and quits.
void ReviewChultProps(AActor* Owner, AChuckCharacter* Chuck, int32 Failures);

// The measurement alone (the audit's numbers): floating trees, logs open or walk-through, jars in stone.
int32 CheckChultProps(UWorld* World, bool bLog);
