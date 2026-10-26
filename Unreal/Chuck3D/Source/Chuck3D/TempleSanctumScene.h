#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TempleSanctumScene.generated.h"

class ACameraActor;
class AChuckCharacter;
class ATempleHero;
class ATempleSanctumBattle;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;

/**
 * The sanctum's two cutscenes (Claude, 2026-10-26; docs/CHULT-TEMPLE-BEHOLDER.md#the-cutscenes), after the 2D game's
 * Phase 6 final chamber: no player control, letterboxed, Esc skips.
 *
 * - The entrance (about 11 s, once a session): as he first comes out onto the gallery the view leaves him and takes in
 *   the battle he has walked into: over his shoulder down into the hall, low behind the heroes up at the beholder, then
 *   the fighter as he calls "Hold the line!" (the heroes' first line, TempleHeroTalk.cpp). Then he has the fight to survive.
 * - The Fireball: when the heroes' argument is due (their lines 4-8), the view goes to them. The wizard has turned from
 *   the sea tear to the beholder and is drawing fiery sigils round himself; the ranger knows them, the fighter fears the
 *   rift, he will try. "......": a mote of fire swells at his staff. "FIREBALL!!": he throws it; it streaks to the
 *   beholder and bursts round it, the Astral under the floor erupting with it, and the blast rolls across the hall to
 *   Chuck. White. As the 2D game, the blast leaves him at (at most) half Sanity and throws him into the rubble map
 *   (ChultTempleRubble.h), where he comes to as the white fades. The music stops at the blast; the temple's returns later.
 *
 * Shots are framed from where the cast stands each frame. The fire is the beholder's flare and spark materials
 * (Tools/create_beholder_fx.py) on the battle's planes; the sounds are the battle's own.
 */
UCLASS()
class CHUCK3D_API ATempleSanctumScene : public AActor
{
    GENERATED_BODY()
public:
    ATempleSanctumScene();
    virtual void Tick(float DeltaSeconds) override;
    void Setup(ATempleSanctumBattle* InBattle);

    enum class EKind : uint8 { None, Entrance, Fireball };
    /** His first coming into the sanctum this session. False if one is playing or there is nobody to film. */
    bool StartEntrance();
    /** The argument is due. False (and nothing begins) while the beholder is in the middle of an attack. */
    bool StartFireball();
    /** Esc. */
    void Skip();
    bool IsPlaying() const { return Kind != EKind::None; }
    bool IsFireball() const { return Kind == EKind::Fireball; }
    /** The bars are up (the scene is in charge of the view). */
    bool IsLetterboxed() const { return Kind != EKind::None && !bReleased; }
    EKind GetKind() const { return Kind; }
    float GetClock() const { return Clock; }
    const TCHAR* GetShotName() const { return Shot; }

    // Tests and their evidence.
    int32 GetEntrances() const { return Entrances; }
    int32 GetFireballs() const { return Fireballs; }
    int32 GetSkips() const { return Skips; }
    int32 GetDetonations() const { return Detonations; }
    int32 GetThrows() const { return Throws; }
    float GetDetonateClock() const { return DetonateAt; }
    int32 GetSanityAfter() const { return SanityAfter; }
    int32 GetFlaresAlive() const { return Flares.Num(); }
    int32 GetMostFlares() const { return MostFlares; }

    // The entrance's beats (s).
    static constexpr float EntranceHall = 3.4f;
    static constexpr float EntranceFloor = 7.2f;
    static constexpr float EntranceEnd = 11.2f;
    // The Fireball's, from the blast (s): the bloom, the blast at him, white, the rubble, the white lifting, control back.
    static constexpr float BlastWide = 1.15f;
    static constexpr float BlastWhite = 1.75f;
    static constexpr float BlastThrow = 2.1f;
    static constexpr float BlastFadeIn = 3.1f;
    static constexpr float BlastControl = 3.7f;
    static constexpr float BlastEnd = 5.8f;
    static constexpr float BeadSpeed = 2300.f;
private:
    TWeakObjectPtr<ATempleSanctumBattle> Battle;
    TWeakObjectPtr<AChuckCharacter> Chuck;
    UPROPERTY() ACameraActor* Camera = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* FlareFx = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* SparkFx = nullptr;
    UPROPERTY() UPointLightComponent* BlastLight = nullptr;
    UPROPERTY() UPointLightComponent* FrontLight = nullptr;
    UPROPERTY() UPointLightComponent* GemLight = nullptr;
    UPROPERTY() UPointLightComponent* AstralLight = nullptr;
    EKind Kind = EKind::None;
    float Clock = 0, ShotClock = 0, DetonateAt = -1, CastAt = -1, BeadAt = -1, SkipAt = -1, WaitClock = 0;
    bool bReleased = false, bThrown = false, bSkipping = false, bCastPlayed = false, bGatherSound = false, bReady = false;
    int32 Entrances = 0, Fireballs = 0, Skips = 0, Detonations = 0, Throws = 0, SanityAfter = -1, MostFlares = 0, Respawns0 = 0;
    const TCHAR* Shot = TEXT("");
    FVector BeadPos = FVector::ZeroVector, BlastAt = FVector::ZeroVector, ChuckAt = FVector::ZeroVector, Eye = FVector::ZeroVector, Aim = FVector::ZeroVector;
    bool bCut = true;
    FRandomStream Random;

    struct FFlareP { FVector Pos, Vel; FLinearColor Col; float Size, Grow, Age, Life, Frame, Rate, Spin, Drag, Rise; };
    struct FMoteP { FVector Pos, Vel; FLinearColor Col; float Size, Age, Life, Drag; };
    TArray<FFlareP> Flares;
    TArray<FMoteP> Motes;
    TArray<FMoteP> SigilPts;   // drawn this frame only
    void Flare(const FVector& At, const FVector& Vel, const FLinearColor& Col, float Size, float Grow, float Life, float Drag = 1.5f, float Rise = 0.f);
    void Mote(const FVector& At, const FVector& Vel, const FLinearColor& Col, float Size, float Life, float Drag = 2.f);
    void TickFx(float Dt);

    void Begin(EKind InKind);
    void Release();          // control back to him (the view, his input)
    void Finish();
    void TickEntrance(float Dt);
    void TickFireball(float Dt);
    void Detonate();
    void Throw();            // white: half Sanity, into the rubble
    void Frame(const FVector& InEye, const FVector& InAim, const TCHAR* Name, float Fov = 50.f);
    void Sigils(const FVector& Feet, const FVector& Tip, float Strength);
    ATempleHero* Hero(int32 I) const;
};
