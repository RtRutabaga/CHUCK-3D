#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CigarettePickup.generated.h"

class UStaticMeshComponent;
class USoundBase;

/**
 * A loose cigarette, the Chuck game's currency (user 2026-09-30: shred grass
 * and break jars to collect the cigarettes inside, like Zelda's rupees). It
 * pops out of what broke, lands and lies on the ground - a fresh, unlit
 * cigarette, no glow or spin - and Chuck pockets it by walking over it.
 */
UCLASS()
class CHUCK3D_API ACigarettePickup : public AActor
{
    GENERATED_BODY()
public:
    ACigarettePickup();
    virtual void Tick(float DeltaSeconds) override;
    /** Spawn one flying out of a broken thing at From (Launch: initial velocity). */
    static ACigarettePickup* Spawn(UWorld* World, const FVector& From, const FVector& Launch, float GroundZ);
    // Pocketed when Chuck's centre comes this close (horizontal, cm), once it can be.
    static constexpr float CollectRadius = 26.f;
    static constexpr float CollectDelay = .35f;   // it visibly pops out first
    /** Count cigarettes hopping out of Base, spread round, landing on Ground. */
    static void Burst(UWorld* World, const FVector& Base, int32 Count, float Ground);
    /** Live pickups (tests). */
    static int32 CountInWorld(UWorld* World);
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY() UStaticMeshComponent* Mesh;
    UPROPERTY() TArray<USoundBase*> PickupSounds;
    FVector Velocity = FVector::ZeroVector;
    FRotator Spin = FRotator::ZeroRotator;
    float Ground = 0;
    float Age = 0;
    bool bLanded = false;
};
