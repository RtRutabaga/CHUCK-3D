#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChuckBreakable.h"
#include "ShipStorm.generated.h"

// The pirate ship's ending begins (Claude, 2026-11-03; docs/SHIP.md "The chest, the trigger and the storm"). After the 2D
// game's Phase 7: the captain's chest in his cabin opens once (scratched or interacted with) and drops a golden carton of
// 40 cigarettes (Premium Buhetian Halfling Leaf), which also starts the golden leaf's 45 s stamina lock. The ending's
// trigger is the 2D game's seven conditions: the chef has seen him, he has talked to the seated pirate, the concertina
// player, the cheering pirate, the dancer and Jeffries, and he has opened the chest. When the last one is met, a storm rolls
// in (user 2026-11-03: "a storm to roll in and for the ship going through a storm to be a thing for a good while before
// the captain kicks chuck off the plank"): the wind rises and the sky closes over, the sea builds into a big swell and the
// ship rolls deeper, rain comes on in squalls, lightning strikes round her with its thunder, spray bursts over the bow and
// the weather rail. The plank comes later; until then the storm keeps on.

/** The ending's seven conditions (the 2D game's gate). They last for the session, through deaths aboard. */
namespace ShipGate
{
    enum class EStep : uint8 { Chef, Seated, Concertina, Cheering, Dancer, Jeffries, Chest, Count };
    constexpr int32 Steps = int32(EStep::Count);
    const TCHAR* StepName(EStep Step);
    void Mark(EStep Step);
    bool Done(EStep Step);
    int32 DoneCount();
    /** All seven met: the storm (and later the captain's sequence) begins. */
    bool IsOpen();
    /** Seconds since the gate opened (world time; -1 while shut). */
    float OpenedFor(const UWorld* World);
    /** Mark every condition (the Ship Storm dev checkpoint and the tests). */
    void OpenAll();
    /** Forget them all, close the chest, end the storm (a fresh start aboard from the menu). */
    void Reset(UWorld* World);
}

/** The captain's chest (in his cabin, against the port side). Its body is the ship's; this is its lid on its hinge, the
 *  glow inside and the carton it drops. Scratched or interacted with, it opens once. */
UCLASS()
class CHUCK3D_API AShipChest : public AChuckBreakable
{
    GENERATED_BODY()
public:
    AShipChest();
    virtual void Tick(float DeltaSeconds) override;
    static AShipChest* Spawn(UWorld* World);
    static AShipChest* Get(const UWorld* World);
    /** Struck by his slash: it opens. */
    virtual void Break(const FVector& Swing) override;
    /** He is close enough, and facing it, to open it with Interact. */
    bool CanInteract(const AActor* Chuck) const;
    void Open();
    bool IsOpen() const { return bOpen; }
    float GetLidAngle() const { return LidAngle; }
    /** Shut it again, the carton gone (ShipGate::Reset). */
    void Close();
private:
    void BuildLid();
    UPROPERTY() class USceneComponent* Hinge = nullptr;
    UPROPERTY() class UProceduralMeshComponent* Lid = nullptr;
    UPROPERTY() class UPointLightComponent* Glow = nullptr;
    TWeakObjectPtr<class ACigarettePickup> Carton;
    bool bOpen = false, bDropped = false;
    float OpenClock = 0.f, LidAngle = 0.f;
};

/** The storm. Spawned with the ship; idle until the gate opens. AShipSetting reads its levels for the sky, the light, the
 *  sea, the rocking and the sails; the rain, the spray, the lightning and the storm's sounds are its own. */
UCLASS()
class CHUCK3D_API AShipStorm : public AActor
{
    GENERATED_BODY()
public:
    AShipStorm();
    virtual void Tick(float DeltaSeconds) override;
    static AShipStorm* Spawn(UWorld* World);
    static AShipStorm* Get(const UWorld* World);
    /** The storm's parts now, 0..1: the wind, the dark (cloud and light), the sea's swell, the rain (squalls over it). */
    float GetWind() const { return Wind; }
    float GetDark() const { return Dark; }
    float GetSea() const { return Sea; }
    float GetRain() const { return Rain; }
    /** A lightning flash's brightness now (0: none; ~1 a far strike, up to ~4 a near one) and where it comes from (a unit
     *  vector from the ship toward the strike, a little up). */
    float GetFlash() const { return Flash; }
    FVector GetFlashFrom() const { return FlashFrom; }
    /** Seconds of storm so far. */
    float GetClock() const { return Clock; }
    int32 GetStrikes() const { return Strikes; }
    int32 GetThunders() const { return Thunders; }
    int32 GetSprays() const { return Sprays; }
    int32 GetDropsShown() const { return DropsShown; }
    int32 GetSplashes() const { return SplashesMade; }
    /** The storm's loops' levels now: rain on deck, rain drumming overhead below, the gale, the wind heard below, the flapping canvas. */
    FVector4 GetLoopLevels() const { return FVector4(LoopNow[0], LoopNow[1], LoopNow[2], LoopNow[3]); }
    float GetFlapLevel() const { return LoopNow[4]; }
    float GetSeaLevel() const { return LoopNow[5]; }
    /** Back to calm (ShipGate::Reset). */
    void Calm();
    /** (Tests and stills) jump the storm to this many seconds in. */
    void SetClock(float Seconds) { Clock = Seconds; bStarted = Seconds > 0.f; }
    /** (Tests and stills) a strike now, Distance cm away at Bearing degrees (ship frame). */
    void StrikeNow(float Distance, float Bearing);
    /** (Tests and stills) a sea thrown over the side now at At (world). */
    void SprayNow(const FVector& At, float Size) { SprayBurst(At, Size); }
private:
    void Levels(float Dt);
    void TickRain(float Dt, const FVector& Camera, bool bInside);
    void TickSpray(float Dt, const FVector& Camera);
    void TickLightning(float Dt);
    void TickSound(float Dt, bool bBelow);
    void BuildBolt(const FVector& Base, const FVector& Camera);
    void Thunder(float Distance, bool bNear);
    void SprayBurst(const FVector& At, float Size);
    class UInstancedStaticMeshComponent* MakeSprites(const TCHAR* Name, const TCHAR* Material, int32 CustomFloats);
    bool bStarted = false;
    float Clock = 0.f, Wind = 0.f, Dark = 0.f, Sea = 0.f, Rain = 0.f, Squall = 0.f, Gust = 0.f;
    // Rain: drops in a column round the view, recycled as they reach the deck or the sea; splashes where they land on deck.
    struct FDrop { FVector P; float Speed; float Len; };
    TArray<FDrop> Drops;
    struct FSplash { FVector P; float Age; float Size; };
    TArray<FSplash> Splashes;
    // Spray: blobs of water thrown up over the bow or the weather rail, blown across the deck and falling.
    struct FBlob { FVector P, V; float Age, Life, Size; };
    TArray<FBlob> Blobs;
    float NextSpray = 6.f;
    // Lightning.
    float NextStrike = 18.f, StrikeAge = 99.f, StrikeBright = 0.f, Flash = 0.f;
    FVector FlashFrom = FVector(0, 1, .4f);
    TArray<float> Pulses;
    struct FPendingThunder { float At; float Distance; bool bNear; };
    TArray<FPendingThunder> PendingThunder;
    int32 Strikes = 0, Thunders = 0, Sprays = 0, DropsShown = 0, SplashesMade = 0;
    UPROPERTY() class UInstancedStaticMeshComponent* RainSprites = nullptr;
    UPROPERTY() class UInstancedStaticMeshComponent* SplashSprites = nullptr;
    UPROPERTY() class UInstancedStaticMeshComponent* SpraySprites = nullptr;
    UPROPERTY() class UProceduralMeshComponent* Bolt = nullptr;
    UPROPERTY() class UMaterialInstanceDynamic* RainMid = nullptr;
    UPROPERTY() class UMaterialInstanceDynamic* SplashMid = nullptr;
    UPROPERTY() class UMaterialInstanceDynamic* SprayMid = nullptr;
    UPROPERTY() class UMaterialInstanceDynamic* BoltMid = nullptr;
    UPROPERTY() class UMaterialInstanceDynamic* BoltGlowMid = nullptr;
    // The loops: rain on deck, rain overhead (below decks), the gale, the wind heard below, the canvas flogging, the heavy sea.
    UPROPERTY() TArray<class UAudioComponent*> Loops;
    float LoopNow[6] = {};
    UPROPERTY() TArray<class USoundBase*> NearThunder;
    UPROPERTY() TArray<class USoundBase*> FarThunder;
    UPROPERTY() TArray<class USoundBase*> Slams;
};

/** The storm's wind now (0 calm .. 1 the full gale), for the sails, the flag and the hanging things (ShipDeck.inl). */
float ShipStormWind();
