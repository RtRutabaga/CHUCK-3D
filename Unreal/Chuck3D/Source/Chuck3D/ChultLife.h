#pragma once
#include "CoreMinimal.h"

class UWorld;

// Chult's hostile life (user 2026-10-14): fifteen zombies scattered through
// the forest on the landing's side of the jungle, none near the brachiation
// trees, and four more raptors on the trees' round beside the first (five).
// Not in the automated traversal tests (their worlds stay as they were), only
// in play and in -ChuckJungleLifeTest.
void SpawnChultLife(UWorld* World);
// Local (to the Chult floor origin) homes of the jungle zombies; filled by SpawnChultLife.
const TArray<FVector>& ChultZombieHomes();

// -ChuckJungleLifeTest: counts and placements, everything left alone stays
// put (zombies idle at home, raptors on their round), a zombie notices him
// and comes; frames with -ChuckJungleLifeCapture; then quits.
void StartChultLifeReview(UWorld* World);

// -ChuckZombieGrabTest: the grab every time (ADockNPC::GetZombieGrabChance): the nearest
// jungle zombie seizes him, lifts him, bites, he vanishes from its hands and is summoned
// back whole at the landing; frames from its side with -ChuckZombieGrabCapture; then quits.
void StartZombieGrabReview(UWorld* World);
