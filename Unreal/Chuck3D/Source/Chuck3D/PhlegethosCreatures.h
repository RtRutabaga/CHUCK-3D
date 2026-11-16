#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhlegethosCreatures.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class USoundBase;
class APhlegethosSetting;

// Phlegethos's creatures (Claude, 2026-11-16; docs/regions/PHLEGETHOS.md#creatures), after the 2D game's Phase 8
// bestiary less its horned devils (the user: "in 2d we had both horned devils and pit fiend but i dont want the
// separate horned devils in our game").

/**
 * A flameskull (2D: entities/flameskull.py): a human skull wreathed in fire, weaving fast back and forth across a fixed
 * haunt with a quicker weave across that and a bob - bee-like, never a pursuer. It floats over the lava as readily as
 * over stone, so it is at its worst where the way is narrow. It cannot be scratched; a touch burns him (a Sanity point)
 * and throws him a step back along the way (back, not off the side: the road is a spine over the lava). A hazard to be
 * timed. On the lava road where the geysers were, and on the ascent's ramp where the bombs fell; a few more weave out
 * over the lava as the setting's life, out of his reach.
 *
 * The skull is the temple's (CDmir's CC0 "human skull 0", real size, SM_TempleSkull); its fire is the game's flipbook
 * fire (AChuckFlames), moving with it so its sparks trail behind; its eyes glow; its own light flickers on the ground.
 */
UCLASS()
class CHUCK3D_API APhlegFlameskull : public AActor
{
    GENERATED_BODY()
public:
    APhlegFlameskull();
    virtual void Tick(float DeltaSeconds) override;
    struct FHaunt
    {
        FVector Home = FVector::ZeroVector;   // world: the middle of its run
        FVector Axis = FVector::ForwardVector;  // the run's direction (horizontal)
        float Range = 240.f;                  // out from Home each way, cm
        float Weave = 40.f, Bob = 12.f;       // across it and up and down
        float Rate = 1.3f;                    // rad/s along the run (2D: 62 / 40 = 1.55)
        bool bHarmful = true;                 // false: out over the lava, beyond reach
    };
    void Init(const FHaunt& InHaunt, float Phase);
    const FHaunt& GetHaunt() const { return Haunt; }
    /** Where its skull's middle is now (world). */
    FVector GetCentre() const { return GetActorLocation(); }

    static constexpr float Radius = 12.f;    // the burning skull, for a touch
    static constexpr int32 Burn = 1;         // of his 5 (2D: 16 of 100)
    /** Tests: no touch burns while set (the traversal trials). */
    static bool bCalm;
    static int32 Burns;                      // touches that burned him (every skull, since the game began)
    static const TArray<TWeakObjectPtr<APhlegFlameskull>>& All();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() UStaticMeshComponent* Skull = nullptr;
    UPROPERTY() UStaticMeshComponent* EyeL = nullptr;
    UPROPERTY() UStaticMeshComponent* EyeR = nullptr;
    UPROPERTY() UPointLightComponent* Light = nullptr;
    UPROPERTY() USoundBase* PassSound = nullptr;
    UPROPERTY() USoundBase* BurnSound = nullptr;
    FHaunt Haunt;
    float Clock = 0, Phase = 0, PassWait = 0, Seed = 0;
    int32 Flame = -1;
    bool bWasNear = false, bAsleep = false;
    FVector Last = FVector::ZeroVector;
    FRotator Facing = FRotator::ZeroRotator;
    FVector Place(float T) const;
};

/** Every creature of the stage, once, after it is built (APhlegethosSetting::Enter). */
void SpawnPhlegethosCreatures(APhlegethosSetting* Stage);
/** How many flameskulls weave on the stage, and how many of them can reach him. */
int32 PhlegethosFlameskullCount(bool bHarmfulOnly);
