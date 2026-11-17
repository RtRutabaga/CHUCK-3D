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

/**
 * A spined devil (2D: entities/spined_devil.py): a small devil that never moves. It crouches on a basalt column beside the
 * way and flicks burning spines from its tail across a lane on a fixed beat (every 2 s, staggered so a row never throws
 * together). A spine that meets him burns a Sanity point and knocks him on along its flight; it burns out against the
 * stone. Brushing it only hurts; scratching it defeats him at once (2D: melee is fatal - solve it by timing, never by
 * fighting). On the gorge's shelf (from columns in the river) and beside the wastes' path.
 *
 * Modelled by Tools/build_phleg_creatures.py: body, head, wings and tail each pivot at their joint; the head watches
 * him, the wings stir and beat as it throws, the tail winds back and whips over its shoulder.
 */
UCLASS()
class CHUCK3D_API APhlegSpinedDevil : public AActor
{
    GENERATED_BODY()
public:
    APhlegSpinedDevil();
    virtual void Tick(float DeltaSeconds) override;
    struct FPerch
    {
        FVector Seat = FVector::ZeroVector;   // world: where it crouches, the column's top
        float Yaw = 0;                        // it faces across the lane
        FVector Aim = FVector::ZeroVector;    // world: the point across the lane its spines fly through (his chest height)
        float Delay = 0;                      // its beat's offset, s
        float ColumnDrop = 460;               // the column's height under the seat (its foot under the lava or the ground)
    };
    void Init(const FPerch& InPerch);
    const FPerch& GetPerch() const { return Perch; }
    /** His scratch from Location facing Ahead, within Reach: fatal (true if it reached it). */
    bool TryScratch(class AChuckCharacter* Chuck, const FVector& Location, const FVector& Ahead, float Reach);
    FVector GetCentre() const;
    static constexpr float Interval = 2.f;       // 2D: SPINED_DEVIL_INTERVAL
    static constexpr float SpineSpeed = 950.f;   // cm/s (2D: 96 px/s; here a dart, but seen leaving the tail)
    static constexpr int32 SpineBurn = 1;        // of his 5 (2D: 18 of 100)
    static int32 Thrown;
    static int32 Hits;
    static int32 Scratched;
    static const TArray<TWeakObjectPtr<APhlegSpinedDevil>>& All();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() UStaticMeshComponent* Column = nullptr;
    UPROPERTY() UStaticMeshComponent* Body = nullptr;
    UPROPERTY() UStaticMeshComponent* Head = nullptr;
    UPROPERTY() UStaticMeshComponent* WingL = nullptr;
    UPROPERTY() UStaticMeshComponent* WingR = nullptr;
    UPROPERTY() UStaticMeshComponent* Tail = nullptr;
    UPROPERTY() TArray<UStaticMeshComponent*> SpineMeshes;
    UPROPERTY() UPointLightComponent* Light = nullptr;
    UPROPERTY() USoundBase* ThrowSound = nullptr;
    UPROPERTY() USoundBase* StoneSound = nullptr;
    UPROPERTY() USoundBase* BurnSound = nullptr;
    UPROPERTY() USoundBase* HissSound = nullptr;
    struct FSpine { FVector Pos = FVector::ZeroVector, Vel = FVector::ZeroVector; float Life = -1.f, Stuck = -1.f; int32 Flame = -1; };
    TArray<FSpine> Flying;
    FPerch Perch;
    float Clock = 0, NextThrow = 0, FlickT = -1, Flap = 0, LookYaw = 0, LookPitch = 0, TouchWait = 0;
    bool bAsleep = false, bReleased = false;
    FVector TailTip() const;
    void Throw();
    void TickSpines(float Dt, class AChuckCharacter* Chuck);
};

/** Every creature of the stage, once, after it is built (APhlegethosSetting::Enter). */
void SpawnPhlegethosCreatures(APhlegethosSetting* Stage);
/** How many flameskulls weave on the stage, and how many of them can reach him. */
int32 PhlegethosFlameskullCount(bool bHarmfulOnly);
/** His scratch against every spined devil in reach (ChuckCharacter's slash). True if one was scratched. */
bool PhlegethosScratchDevils(class AChuckCharacter* Chuck, const FVector& Location, const FVector& Ahead, float Reach);
/** The stage's lemures, and its Pit Fiend (null if none). */
int32 PhlegethosLemureCount();
class ATempleSkeleton* PhlegethosPitFiend();
