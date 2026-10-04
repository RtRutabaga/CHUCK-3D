#pragma once
#include "CoreMinimal.h"
class UWorld;

/**
 * Low cargo crates the right height for Chuck's speed vault (user 2026-10-04):
 * 40-48 cm high and 40 cm deep, each standing alone with a clear run-up and
 * landing either side along one line (found by a scan of the docks for 6 m of
 * open flat ground, kept off the world checks' walking routes).
 */
struct FVaultCrate
{
    FVector Centre;     // on the ground
    FVector Run;        // the open line across it (horizontal unit)
    float Height;
};
void SpawnVaultCrates(UWorld* World);
const TArray<FVaultCrate>& GetVaultCrates();
