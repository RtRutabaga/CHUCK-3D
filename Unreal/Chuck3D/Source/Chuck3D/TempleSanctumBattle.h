#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChultTempleInterior.h"
#include "TempleSkeleton.h"
#include "TempleSanctumBattle.generated.h"

class UAnimSequence;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class USkeletalMesh;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class ATempleHero;
class ATempleBeholder;
class ATempleSeal;
class AChuckCharacter;
class UAudioComponent;

/**
 * The sanctum's battle, before the beholder (user 2026-10-22: "add them to the grand temple room at the
 * end. we'll add the beholder later"; docs/CHULT-TEMPLE-HEROES.md). Spawns the three heroes on the
 * hall's floor before the dais, facing where the beholder will hang over it, and keeps skeletons coming
 * at them out of the bones: twelve lie in reserve round the hall's dais end, and while Chuck is in the
 * sanctum three at a time get up and go for a hero (mostly the fighter, the 2D game's "fighter occupies
 * skeletons"). The fallen lie where they fell a while, then go quietly back into reserve where he is not
 * looking. It owns what flies: the ranger's arrows, the wizard's bolts, struck-off skulls, bone splinters
 * and dust, and the heroes' fight sounds.
 */
UCLASS()
class CHUCK3D_API ATempleSanctumBattle : public AActor
{
    GENERATED_BODY()
public:
    ATempleSanctumBattle();
    virtual void Tick(float DeltaSeconds) override;
    using EBlow = ATempleSkeleton::EBlow;

    const TArray<TWeakObjectPtr<ATempleSkeleton>>& Foes() const { return Pool; }
    ATempleHero* GetHero(int32 Index) const { return Heroes.IsValidIndex(Index) ? Heroes[Index].Get() : nullptr; }
    /** Where the beholder will hang (the heroes face it): over the front of the dais. */
    FVector BeholderSpot() const { return Beholder; }
    /** The beholder hanging there (docs/CHULT-TEMPLE-BEHOLDER.md): it fights while the battle is on. */
    ATempleBeholder* GetBeholder() const { return TheBeholder.Get(); }
    /** Tests: let the beholder fight by itself while the battle is on (default), or hold it to scripted orders. */
    void SetBeholderFights(bool b) { bBeholderFights = b; }
    /** The wizard's seal over the sea tear (docs/CHULT-TEMPLE-HEROES.md#sealing-the-sea-tear). */
    ATempleSeal* GetSeal() const { return TheSeal.Get(); }
    float FloorZ() const { return Floor; }
    bool IsActive() const { return bActive; }
    /** Its assets are in and the cast is on the floor. */
    bool IsReady() const { return bReady; }
    UMaterialInterface* GetTrailMaterial() const { return TrailMat; }
    UStaticMesh* GetArrowMesh() const { return ArrowMesh; }

    enum class ESound : uint8 { Whoosh, BladeBone, ShieldBlock, ArmourHit, ArmourStep, ShieldBash, BowDraw, BowRelease, ArrowFlight, ArrowHit, BoltCast, BoltImpact, SkullBounce, BodyHit,
        // The beholder's (Tools/gen_beholder_sfx.py). Its five rays in EBeholderRay's order.
        BeholderRayCharge, BeholderRayDisintegrate, BeholderRayEnervate, BeholderRayTelekinetic, BeholderRayParalyze, BeholderRayFear,
        BeholderRayImpact, BeholderDisintegrate, BeholderConeCharge, BeholderConeBlast, BeholderRoar, BeholderSnap, BeholderGrowl, BeholderHurt,
        // The wizard's seal over the sea tear (2026-10-25).
        SealSurge, SealBreak, Count };
    void Sound(ESound Kind, const FVector& At, float Volume = 1.f, float Pitch = 1.f);
    USoundBase* HumSound() const { return Hum; }
    USoundAttenuation* GetAttenuation() const { return Attenuation; }

    /** A hero's blade (or shield) landed on Victim at At, moving along Vel (cm/s): sound, splinters, dust, the skull if struck off. */
    void OnBladeHit(ATempleSkeleton* Victim, EBlow Kind, const FVector& At, const FVector& Vel);
    void OnBlocked(const FVector& At, const FVector& Normal);
    void FireArrow(const FVector& From, ATempleSkeleton* At, const FVector& Fallback);
    void FireBolt(const FVector& From, ATempleSkeleton* At);
    // What flies, for the beholder too: bone splinters, dust, the bolts' fire.
    void Splinters(const FVector& At, const FVector& Dir, int32 Count, float Speed);
    void Dust(const FVector& At, const FVector& Dir, int32 Count, float Size);
    void FirePuffs(const FVector& At, const FVector& Vel, int32 Count, float Size);
    int32 GetBeholderArrows() const { return BeholderArrows; }
    int32 GetBeholderBolts() const { return BeholderBolts; }
    int32 GetFizzled() const { return Fizzled; }

    // Tests and captures.
    void SetForcedActive(bool b) { bForced = b; }
    void SetWaves(bool b) { bWaves = b; }
    ATempleSkeleton* WakeOne(ATempleHero* For, const FVector* At = nullptr);
    int32 GetUp() const;
    int32 GetFallen() const { return Fallen; }
    int32 GetDecapitations() const { return Decapitations; }
    int32 GetArrowsFlying() const { return Arrows.Num(); }
    int32 GetBoltsFlying() const { return Bolts.Num(); }
    int32 GetChips() const { return Chips.Num(); }
    int32 GetSkulls() const { return Skulls.Num(); }
    int32 GetSoundsPlayed() const { return SoundsPlayed; }
    void ClearFlying();

    static ATempleSanctumBattle* Get(const UWorld* World);

    // The heroes' lines (TempleHeroTalk.cpp, user 2026-10-25): their calls when Chuck comes in, then the argument before the Fireball.
    /** A hero is speaking one of the scene's lines (the music ducks under it). */
    bool IsHeroSpeaking() const;
    /** Its sound is playing now. */
    bool IsTalkSoundPlaying() const;
    /** The line's current subtitle cue and who says it. */
    bool GetHeroSubtitle(FString& Speaker, FString& Text) const;
    /** Tests: from the start again, the waits before the lines scaled by GapScale. */
    void RestartTalk(float GapScale = 1.f);
    /** The line playing (index into the script), or -1; the next line's index; lines started since the restart. */
    int32 GetTalkPlaying() const { return TalkPlaying; }
    int32 GetTalkStep() const { return TalkStep; }
    int32 GetTalkSpoken() const { return TalkSpoken; }
    static int32 TalkLineCount();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    // Assets, hard-referenced so they cook.
    UPROPERTY() TArray<USkeletalMesh*> HeroMeshes;
    UPROPERTY() TArray<UAnimSequence*> HeroClipList;
    UPROPERTY() USkeletalMesh* BeholderMesh = nullptr;
    TArray<TMap<FName, UAnimSequence*>> HeroClips;
    UPROPERTY() UStaticMesh* ArrowMesh = nullptr;
    UPROPERTY() UStaticMesh* SkullMesh = nullptr;
    UPROPERTY() UStaticMesh* Cube = nullptr;
    UPROPERTY() UStaticMesh* Plane = nullptr;
    UPROPERTY() UStaticMesh* Sphere = nullptr;
    UPROPERTY() UMaterialInterface* BoneMat = nullptr;
    UPROPERTY() UMaterialInterface* DustMat = nullptr;
    UPROPERTY() UMaterialInterface* BoltFireMat = nullptr;
    UPROPERTY() UMaterialInterface* GlowMat = nullptr;
    UPROPERTY() UMaterialInterface* TrailMat = nullptr;
    UPROPERTY() TArray<UMaterialInterface*> BeholderFx;   // beam, wave, spark, flare (Tools/create_beholder_fx.py)
    UPROPERTY() USoundBase* Hum = nullptr;
    UPROPERTY() TArray<USoundBase*> SoundList;
    TArray<TArray<USoundBase*>> Sounds;
    UPROPERTY() USoundAttenuation* Attenuation = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* ChipFx = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* DustFx = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* ArrowFx = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* BoltFireFx = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* BoltCoreFx = nullptr;
    UPROPERTY() UPointLightComponent* Flash = nullptr;
    UPROPERTY() TArray<UPointLightComponent*> BoltLights;
    UPROPERTY() TArray<UStaticMeshComponent*> SkullComps;

    TArray<TWeakObjectPtr<ATempleHero>> Heroes;
    TWeakObjectPtr<ATempleBeholder> TheBeholder;
    TWeakObjectPtr<ATempleSeal> TheSeal;
    UPROPERTY() UMaterialInterface* SealWebMat = nullptr;
    TArray<TWeakObjectPtr<ATempleSkeleton>> Pool;
    TArray<FTempleSkeletonPost> Reserve;
    FVector Beholder = FVector::ZeroVector;
    float Floor = 0, NextWake = 0, FlashTime = 0;
    bool bActive = false, bForced = false, bWaves = true, bTickModeSet = false, bBeholderFights = true;
    int32 Fallen = 0, Decapitations = 0, SoundsPlayed = 0, WakeCount = 0, BeholderArrows = 0, BeholderBolts = 0, Fizzled = 0;
    FRandomStream Random;

    struct FChip { FVector Pos, Vel; FQuat Rot; FVector Spin; float Size, Age, Life; bool bRest; };
    struct FPuff { FVector Pos, Vel; float Size, Grow, Age, Life, Frame, Rate, Spin, Alpha; bool bFire; };
    struct FArrowFlight { FVector Pos, Vel; TWeakObjectPtr<ATempleSkeleton> Target; float Age; bool bSpent; };
    struct FBoltFlight { FVector Pos, Vel; TWeakObjectPtr<ATempleSkeleton> Target; float Age; };
    struct FSkullFlight { TWeakObjectPtr<ATempleSkeleton> Owner; FVector Pos, Vel; FQuat Rot; FVector Spin; float Age; bool bRest; int32 Bounces; };
    TArray<FChip> Chips;
    TArray<FPuff> Puffs;
    TArray<FArrowFlight> Arrows;
    TArray<FBoltFlight> Bolts;
    TArray<FSkullFlight> Skulls;

    void SpawnCast();
    TArray<FSoftObjectPath> AssetPaths() const;
    void StartLoading();
    void FinishLoading();
    TSharedPtr<struct FStreamableHandle> Loading;
    bool bLoading = false, bReady = false;
    double LoadStart = 0;
    void TickFlying(float DeltaSeconds, const FVector& Eye);
    void Draw(const FVector& Eye);
    void Recycle();

    // The heroes' lines (TempleHeroTalk.cpp).
    UPROPERTY() TArray<USoundBase*> VoiceSounds;
    UPROPERTY() UAudioComponent* VoiceAudio = nullptr;
    UPROPERTY() USoundAttenuation* VoiceAttenuation = nullptr;
    int32 TalkStep = 0, TalkPlaying = -1, TalkSpoken = 0, TalkRespawns = -1;
    float TalkWait = 0.f, TalkElapsed = 0.f, TalkOut = 0.f, TalkGapScale = 1.f;
    static void TalkAssetPaths(TArray<FSoftObjectPath>& Paths);
    void FinishTalkLoading();
    void TickTalk(float DeltaSeconds, bool bIn, const AChuckCharacter* Chuck);
};

/** Spawns the battle once (not in other automated runs). Called with the temple skeletons. */
void SpawnTempleSanctumBattle(UWorld* World);
/** -ChuckSanctumTest: the heroes, their clips and blows, the skeletons' strikes, arrows, bolts; then quit.
 *  -ChuckSanctumCapture also saves frames (Saved/Screenshots/.../Sanctum). */
void StartTempleSanctumReview(UWorld* World);
