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
    const float Start = FMath::FRandRange(0.f, 360.f);
    for (int32 I = 0; I < Cigarettes; ++I)
    {
        const FVector Out = FRotator(0, Start + 360.f * I / FMath::Max(1, Cigarettes) + FMath::FRandRange(-25.f, 25.f), 0).Vector();
        ACigarettePickup::Spawn(GetWorld(), FVector(Base.X, Base.Y, Ground + 10.f), Out * FMath::FRandRange(45.f, 85.f) + FVector(0, 0, FMath::FRandRange(170.f, 230.f)), Ground);
    }
    Cigarettes = 0;
}
void AChuckBreakable::EndPlay(const EEndPlayReason::Type Reason)
{
    Registry.Remove(this);
    Super::EndPlay(Reason);
}
