#pragma once
#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/Actor.h"
#include "TempleBeholder.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USpotLightComponent;
class UPoseableMeshComponent;
class UProceduralMeshComponent;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;
class ATempleHero;
class ATempleSanctumBattle;
class ATempleSkeleton;

/** The beholder's eye rays (D&D 5e's ten, the five that read in the sanctum's fight; docs/CHULT-TEMPLE-BEHOLDER.md#the-fight). */
enum class EBeholderRay : uint8 { Disintegrate, Enervate, Telekinetic, Paralyze, Fear, Count };
/** What it is doing: hovering, loosing rays, the bite, the central eye's cone, reeling from a blow. */
enum class EBeholderMove : uint8 { Hover, Rays, Bite, Cone };

/** A scripted ray (a cutscene, a test): what, at whom (or a point), whether it strikes them, after how long. */
struct FBeholderRayOrder
{
    EBeholderRay Kind = EBeholderRay::Disintegrate;
    TWeakObjectPtr<AActor> Target;     // a hero, a skeleton, Chuck, or none (then Aim)
    FVector Aim = FVector::ZeroVector;
    bool bStrike = true;               // false: it passes by them (they evade)
    float Delay = 0.f;                 // s after the order before the stalk starts to aim
    int32 Stalk = -1;                  // -1: the best placed free stalk
};

/**
 * The beholder of the sanctum (user 2026-10-23: "start creating the beholder, it should look really cool
 * and detailed"; 2026-10-25: "create the attacks and movements the beholder will use ... varied and
 * consistent with dnd 5e beholder attacks, and one of them should be an area of effect attack that can
 * hurt Chuck"; docs/CHULT-TEMPLE-BEHOLDER.md). SK_Beholder (Tools/build_beholder.py) hangs over the front of
 * the dais and fights the heroes:
 *  - eye rays from its stalks (5e: up to three a round at different targets, more as legendary actions):
 *    disintegration (green: a skeleton in its way is dust), enervation (violet), telekinetic (pale blue:
 *    the hero is thrown back), paralysing (gold: held rigid) and fear (crimson: driven back). Each stalk
 *    curls to aim, its eye gathers the ray's light, the beam lances out; the fighter can take one on his
 *    shield, the others evade some. A beam that passes through Chuck costs him a point of Sanity;
 *  - the bite (5e: its one melee attack): a roaring lunge down onto the fighter, jaws snapping on his shield;
 *  - the central eye's cone (the 2D game's "cone of force"; 5e's antimagic cone): it rises, curls its stalks
 *    away (its own rays fail in the cone), its great eye opens wide and gathers, then blasts a cone of
 *    pale force across the hall. Magic in it is snuffed (the wizard's binding gutters, bolts in flight
 *    fizzle), skeletons are flattened, heroes thrown; Chuck, a fey summon held here by magic, loses two
 *    Sanity and is flung, unless he is out of the cone or behind a pier;
 *  - movement: it drifts and banks over the dais between attacks, looms up before the cone, reels from the
 *    heroes' arrows and bolts.
 * Posed in component space by a small forward pass over the rig. The fight runs by itself while the
 * battle is on (SetFighting); every move can also be ordered by a script (the planned entrance and
 * Fireball cutscenes).
 */
UCLASS()
class CHUCK3D_API ATempleBeholder : public AActor
{
    GENERATED_BODY()
public:
    ATempleBeholder();
    virtual void Tick(float DeltaSeconds) override;
    void Setup(USkeletalMesh* Mesh, ATempleSanctumBattle* InBattle, const FVector& Home);
    /** The fight's effects: the beam and wave materials (procedural ribbons), sparks and flares (instanced planes). */
    void SetFx(UMaterialInterface* Beam, UMaterialInterface* Wave, UMaterialInterface* Spark, UMaterialInterface* Flare, UStaticMesh* Plane);

    FVector GetEyeLocation() const;
    /** The body's centre (its head-sphere; the heroes aim at it). */
    FVector GetCentre() const;
    static constexpr float BodyRadius = 100.f;
    FVector GetForward() const { return GetActorForwardVector(); }
    int32 GetPoses() const { return Poses; }
    int32 GetBlinks() const { return Blinks; }
    int32 GetSnarls() const { return Snarls; }
    int32 GetSaccades() const { return Saccades; }
    /** The greatest distance any stalk eye has moved from its rest place (cm): they are alive. */
    float GetStalkTravel() const { return StalkTravel; }
    float GetJawAngle() const { return JawNow; }
    /** Tests: open the jaw in a snarl now; hold the gaze on a point (zero vector: free again). */
    void Snarl();
    void HoldGaze(const FVector& At) { HeldGaze = At; }
    UPoseableMeshComponent* GetBody() const { return Body; }

    // ---- The fight (TempleBeholderFight.cpp) ----
    /** The fight runs by itself (the battle sets this while Chuck is in the hall); off, it only hovers and watches. */
    void SetFighting(bool b) { bFighting = b; }
    bool IsFighting() const { return bFighting; }
    EBeholderMove GetMove() const { return Move; }
    /** Seconds into the current move. */
    float GetMoveTime() const { return T - MoveStart; }
    bool IsBusy() const { return Move != EBeholderMove::Hover || Shots.Num() > 0; }
    /** Scripted rays (a volley if several). Returns how many were taken (stalks free). */
    int32 OrderRays(const TArray<FBeholderRayOrder>& Orders);
    /** Scripted bite on a hero (the fighter). False if busy. */
    bool OrderBite(ATempleHero* Target);
    /** Scripted cone toward a point (on the floor). False if busy. */
    bool OrderCone(const FVector& Toward);
    /** Scripted hover place (world; zero: its own drift again). */
    void OrderHover(const FVector& Where) { ScriptHover = Where; }
    /** Struck by a hero's arrow, bolt or blade at At, the blow travelling along Dir: it reels. */
    void TakeHit(const FVector& At, const FVector& Dir, float Strength);
    /** Within the cone's charged blast? (Apex = the great eye.) Used for its hits and by the bolts it snuffs. */
    bool InCone(const FVector& P) const;
    bool IsConeBlasting() const { return Move == EBeholderMove::Cone && T - MoveStart >= ConeBlastAt && T - MoveStart < ConeBlastAt + ConeTravel + .1f; }

    // The cone's timing (s from its start) for scripts: the loom, the charge, the blast, its travel, the recovery.
    static constexpr float ConeChargeAt = .9f;
    static constexpr float ConeBlastAt = 2.4f;
    static constexpr float ConeTravel = .6f;
    static constexpr float ConeLength = 4.4f;
    static constexpr float ConeRange = 2600.f;
    static constexpr float ConeHalfDeg = 26.f;
    // The bite's: the wind-up, the lunge, the snap, the return.
    static constexpr float BiteLungeAt = .55f;
    static constexpr float BiteSnapAt = .9f;
    static constexpr float BiteLength = 2.45f;
    static const TCHAR* RayName(EBeholderRay Kind);

    // Tests.
    int32 GetRaysFired(EBeholderRay Kind) const { return RaysFired[static_cast<int32>(Kind)]; }
    int32 GetRaysFired() const;
    int32 GetVolleys() const { return Volleys; }
    int32 GetBites() const { return Bites; }
    int32 GetCones() const { return Cones; }
    int32 GetChuckHits() const { return ChuckHits; }
    int32 GetHeroHits() const { return HeroHits; }
    int32 GetHitsTaken() const { return HitsTaken; }
    int32 GetDisintegrated() const { return Disintegrated; }
    float GetClosestBite() const { return ClosestBite; }
    int32 GetConeHits() const { return ConeHits; }
    int32 GetConeChuckHits() const { return ConeChuckHits; }
    int32 GetSheltered() const { return Sheltered; }
    FVector GetConeAxis() const { return ConeAxis; }
    FVector GetConeApex() const { return ConeApex; }
    float GetHomeGap() const;
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY() UPoseableMeshComponent* Body = nullptr;
    UPROPERTY() UPointLightComponent* EyeLight = nullptr;
    UPROPERTY() USpotLightComponent* ConeLight = nullptr;   // the central eye's gaze along its cone: the cone's telegraph
    UPROPERTY() UMaterialInstanceDynamic* EyeMat = nullptr;
    UPROPERTY() UProceduralMeshComponent* BeamMesh = nullptr;
    UPROPERTY() UProceduralMeshComponent* WaveMesh = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* SparkFx = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* FlareFx = nullptr;
    UPROPERTY() TArray<UPointLightComponent*> FxLights;
    UPROPERTY() TArray<UStaticMeshComponent*> StuckArrows;
    TWeakObjectPtr<ATempleSanctumBattle> Battle;

    enum class EKind : uint8 { Root, Body, Jaw, Tongue, LidUpper, LidLower, Eye, Stalk, StalkEye, Other };
    struct FBone
    {
        FName Name; int32 Parent = INDEX_NONE; EKind Kind = EKind::Other; int32 Stalk = -1, Seg = 0;   // Seg: a stalk's or the tongue's segment
        FVector RefPos = FVector::ZeroVector, RefScale = FVector::OneVector; FQuat RefRot = FQuat::Identity;
        FVector Dir = FVector::ForwardVector, Axis1 = FVector::RightVector, Axis2 = FVector::UpVector;
    };
    TArray<FBone> Bones;
    TArray<FQuat> Delta;
    TArray<FVector> Pos;
    struct FStalk
    {
        float Phase1 = 0, Phase2 = 0, Rate1 = .4f, Rate2 = .5f, NextLook = 0;
        FVector Look = FVector::ZeroVector; FQuat Aim = FQuat::Identity; int32 Eye = INDEX_NONE, Root = INDEX_NONE; FVector RestEye = FVector::ZeroVector;
        // Curling to aim a ray (or away from the cone): the turn of the whole stalk (component space) and how far it is taken.
        FQuat Curl = FQuat::Identity; float CurlW = 0.f, CurlWant = 0.f, Kick = 0.f;
        FVector Glow = FVector::ZeroVector; float GlowW = 0.f;   // its eye gathering a ray (colour, strength)
        bool bBusy = false;
    };
    TArray<FStalk> Stalks;
    FVector Home = FVector::ZeroVector, EyeRest = FVector::ZeroVector, HeldGaze = FVector::ZeroVector;
    FQuat EyeAim = FQuat::Identity;
    FVector GazeAt = FVector::ZeroVector;
    float T = 0, NextGaze = 0, NextBlink = 2.f, BlinkStart = -10.f, NextSnarl = 6.f, SnarlStart = -10.f, SnarlLen = 1.f, SnarlOpen = 18.f;
    float JawNow = 0, Yaw = 0, StalkTravel = 0, CloseSign = 1.f;
    int32 Poses = 0, Blinks = 0, Snarls = 0, Saccades = 0;
    FRandomStream Random;

    // The fight's state.
    bool bFighting = false;
    EBeholderMove Move = EBeholderMove::Hover;
    float MoveStart = 0, NextAttack = 3.f, NextDrift = 0, FightClock = 0, LastCone = -100.f, LastBite = -100.f, LastHurtSound = -10.f;
    int32 Script = 0;                  // place in its opening pattern (the fight starts the same way each time)
    FVector HoverAt = FVector::ZeroVector, ScriptHover = FVector::ZeroVector, Place = FVector::ZeroVector, PlaceVel = FVector::ZeroVector;
    FVector Reel = FVector::ZeroVector, ReelVel = FVector::ZeroVector;   // pitch/roll/yaw (deg) thrown by a blow, on a spring
    float WantYaw = 0;
    // Pose layers the moves drive (deg; 0..1 weights).
    float JawExtra = 0, PitchExtra = 0, LidsWide = 0, EyeFlare = 0, StalksBack = 0, Writhe = 1.f;
    TWeakObjectPtr<ATempleHero> BiteTarget;
    FVector BiteFrom = FVector::ZeroVector, BiteStrike = FVector::ZeroVector;
    bool bSnapped = false, bRoared = false, bCountered = false;
    FVector ConeApex = FVector::ZeroVector, ConeAxis = FVector::ForwardVector, ConeToward = FVector::ZeroVector;
    bool bConeCharged = false, bConeBlasted = false;
    float ConeFront = 0;
    TArray<TWeakObjectPtr<AActor>> ConeStruck;

    struct FShot
    {
        EBeholderRay Kind = EBeholderRay::Disintegrate; int32 Stalk = 0;
        TWeakObjectPtr<AActor> Target; FVector Aim = FVector::ZeroVector; bool bStrike = true;
        float Start = 0, Fire = 0, End = 0;   // T: aiming from Start, the beam from Fire to End
        bool bCharged = false, bFired = false, bChuckHit = false;
        FVector From = FVector::ZeroVector, To = FVector::ZeroVector, Dir = FVector::ForwardVector;
        TWeakObjectPtr<AActor> Struck;
        float Seed = 0;
    };
    TArray<FShot> Shots;
    int32 RaysFired[static_cast<int32>(EBeholderRay::Count)] = {};
    int32 Volleys = 0, Bites = 0, Cones = 0, ChuckHits = 0, HeroHits = 0, HitsTaken = 0, Disintegrated = 0, ConeHits = 0, ConeChuckHits = 0, Sheltered = 0;
    float ClosestBite = 1e9f;

    // Effects (TempleBeholderFx.cpp).
    struct FMote { FVector Pos, Vel; FLinearColor Col; float Size, Age, Life, Drag, Grav; bool bFloor; };
    struct FFlare { FVector Pos, Vel; FLinearColor Col; float Size, Grow, Age, Life, Frame, Rate, Spin; };
    struct FAffliction { TWeakObjectPtr<AActor> Who; EBeholderRay Kind; float Until; float Next; };
    struct FFxLight { FVector Pos; FLinearColor Col; float Peak, Age, Life; };
    TArray<FMote> Motes;
    TArray<FFlare> Flares;
    TArray<FAffliction> Afflictions;
    TArray<FFxLight> Flashes;
    bool bFxReady = false;
    FVector CamEye = FVector::ZeroVector;

    void ReadRig();
    void PickGaze();
    FVector Watchable(int32 Pick) const;
    void Pose(float Dt);

    // TempleBeholderFight.cpp
    void TickFight(float Dt);
    void Direct();
    void Drift();
    bool StartVolley(int32 Count);
    void StartShot(EBeholderRay Kind, AActor* Target, const FVector& Aim, bool bStrike, float Delay, int32 Stalk, int32 Order);
    int32 FreeStalk(const FVector& Aim) const;
    void TickShots(float Dt);
    void FireShot(FShot& S);
    FVector StalkEye(int32 Stalk) const;
    FVector StalkEyeForward(int32 Stalk) const;
    void TickBite(float Dt);
    void TickCone(float Dt);
    void ConeStrike(AActor* Who, const FVector& At);
    void Sound(int32 Kind, const FVector& At, float Volume = 1.f, float Pitch = 1.f) const;
    ATempleHero* PickHero(float Fighter, float Wizard) const;
    void StickArrowAt(const FVector& At, const FVector& Dir);
    friend class ATempleSanctumBattle;

    // TempleBeholderFx.cpp
    static FLinearColor RayColour(EBeholderRay Kind);
    void TickFx(float Dt);
    void DrawBeams();
    void DrawWave();
    void Spark(const FVector& At, const FVector& Vel, const FLinearColor& Col, float Size, float Life, float Drag = 2.f, float Grav = 0.f, bool bFloor = false);
    void Burst(const FVector& At, const FVector& Normal, const FLinearColor& Col, int32 Count, float Speed, float Size);
    void Flare(const FVector& At, const FVector& Vel, const FLinearColor& Col, float Size, float Life);
    void Flash(const FVector& At, const FLinearColor& Col, float Peak, float Life);
    void Afflict(AActor* Who, EBeholderRay Kind, float Seconds);
};

/** A camera shake made in code (no assets): noise in location and rotation, eased out. Three strengths. */
UCLASS()
class CHUCK3D_API UBeholderShakePattern : public UCameraShakePattern
{
    GENERATED_BODY()
public:
    UBeholderShakePattern(const FObjectInitializer& ObjectInitializer);
    UPROPERTY() float Duration = .6f;
    UPROPERTY() float LocAmp = 4.f;    // cm
    UPROPERTY() float RotAmp = 1.f;    // deg
    UPROPERTY() float Freq = 18.f;     // Hz
    UPROPERTY() bool bSwell = false;   // grows to its end (a charge) instead of dying away
private:
    float Elapsed = 0.f, Seed = 0.f;
    virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
    virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
    virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
    virtual bool IsFinishedImpl() const override { return Elapsed >= Duration; }
};

UCLASS() class CHUCK3D_API UBeholderShakeBlast : public UCameraShakeBase { GENERATED_BODY() public: UBeholderShakeBlast(const FObjectInitializer& ObjectInitializer); };
UCLASS() class CHUCK3D_API UBeholderShakeJolt : public UCameraShakeBase { GENERATED_BODY() public: UBeholderShakeJolt(const FObjectInitializer& ObjectInitializer); };
UCLASS() class CHUCK3D_API UBeholderShakeCharge : public UCameraShakeBase { GENERATED_BODY() public: UBeholderShakeCharge(const FObjectInitializer& ObjectInitializer); };

/** Shake the player's view for a blow at At: Strength 1 at the spot, fading out to nothing at Radius. Kind 0 jolt, 1 blast, 2 charge. */
void BeholderShake(const UWorld* World, const FVector& At, float Strength, float Radius, int32 Kind);
