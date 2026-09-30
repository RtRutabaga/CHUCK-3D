#include "ChuckBreakable.h"
#include "CigarettePickup.h"

namespace
{
    TArray<TWeakObjectPtr<AChuckBreakable>> Registry;
}

const TArray<TWeakObjectPtr<AChuckBreakable>>& AChuckBreakable::All()
{
    Registry.RemoveAll([](const TWeakObjectPtr<AChuckBreakable>& Entry) { return !Entry.IsValid(); });
    return Registry;
}

void AChuckBreakable::BeginPlay()
{
    Super::BeginPlay();
    Registry.Add(this);
}

void AChuckBreakable::DropContents()
{
    // Each cigarette hops out and lands a little way off, spread round.
    const FVector Base = GetActorLocation();
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckLootGround), false, this);
    const float Ground = GetWorld()->LineTraceSingleByChannel(Hit, Base + FVector(0, 0, 20), Base - FVector(0, 0, 80), ECC_Visibility, Query)
        ? static_cast<float>(Hit.ImpactPoint.Z) : static_cast<float>(Base.Z);
    ACigarettePickup::Burst(GetWorld(), Base, Cigarettes, Ground);
    Cigarettes = 0;
}
void AChuckBreakable::EndPlay(const EEndPlayReason::Type Reason)
{
    Registry.Remove(this);
    Super::EndPlay(Reason);
}
