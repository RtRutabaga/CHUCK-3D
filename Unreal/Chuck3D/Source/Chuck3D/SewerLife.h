#pragma once
#include "CoreMinimal.h"
class UWorld;
class AActor;

/**
 * What lives in the sewer (Claude; user 2026-10-03 sewer plan): rats, first a
 * small group just past the first Astral gap (the 2D game's scratch lesson,
 * References/Original/PHASE-2.md), then pairs further on; and cigarette tufts
 * as damp moss along the wall bases. The wide chamber is left for the zombie.
 * Not placed in the scripted walk-through tests (-ChuckSewerTest,
 * -ChuckStreamTest), where bites would end the run.
 */
void SpawnSewerLife(UWorld* World);
/** Light an actor with the sewer's own lighting channel (sun and sky are off underground). */
void UseSewerLighting(AActor* Actor);
/** What SpawnSewerLife placed, for tests. */
int32 GetSewerRatsPlaced();
int32 GetSewerTuftsPlaced();
/** The route sample index of the first rat group (just past the first gap), for tests. */
int32 GetSewerFirstRatsSample();
/** How many rats were placed in that first group (they wander once placed). */
int32 GetSewerFirstGroupPlaced();
/** The zombie in the wide chamber (null in the walk-through tests). */
class ADockNPC* GetSewerZombie();
int32 GetSewerZombieSample();
