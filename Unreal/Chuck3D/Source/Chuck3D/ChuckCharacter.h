#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ChuckCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UGroomComponent;
class UAnimSequence;
class UChuckAnimInstance;
class USkeletalMesh;
class UStaticMeshComponent;
class USoundBase;
class UInstancedStaticMeshComponent;

UCLASS()
class CHUCK3D_API AChuckCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AChuckCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    /** Snap the orbit to the other framing preset (tests and captures; players orbit freely). */
    void ToggleCamera();
    void ResetToDock();
    /** Roll, or side jump when the stick (x right, y forward, camera-relative) is mostly sideways. */
    void DodgeToward(FVector2D Stick);
    /** Claw slash (LMB / X) pressed: a stepping slash when standing, the upper
     *  body over the stride on the move. Pressing again, or holding, chains
     *  another strike on a steady beat; which paw strikes is random (never three
     *  of one paw in a row), so a flurry isn't a metronomic left-right. */
    void Slash();
    /** Slash button released: the flurry ends after the current paw. */
    void SlashReleased() { bSlashHeld = false; }
    /** Jump pressed: a jump on the ground; on a wall (or just after leaving
     *  one) a wall jump; in the air, buffered for a wall reached just after. */
    void JumpPressed();
    bool IsWallRunning() const { return Gait == EGait::WallRun; }
    int32 GetWallRuns() const { return WallRuns; }
    int32 GetWallJumps() const { return WallJumps; }
    bool IsHanging() const { return Gait == EGait::Hang; }
    int32 GetHangs() const { return Hangs; }
    int32 GetPullUps() const { return PullUps; }
    int32 GetMantles() const { return Mantles; }
    // Ledges: grabbed automatically when his paws reach a top edge while he is
    // moving up or into the wall; holding toward the wall this long pulls up.
    static constexpr float PullUpHold = .2f;
    // Hanging: stick sideways shimmies along the edge.
    static constexpr float ShimmySpeed = 45.f;   // cm/s
    // Falls from higher than this (cm, apex to landing) end in a roll.
    // User 2026-09-29: doubled from 80.
    static constexpr float RollFallHeight = 160.f;
    // Walking gently off an edge with at least this drop below grabs it and hangs (GTA-style).
    static constexpr float DropHangMinDrop = 60.f;
    int32 GetDropHangs() const { return DropHangs; }
    /** Strafe (hold Q/E or LT): facing held down the camera, sidestepping; jump = side jump. */
    bool IsStrafing() const { return Gait == EGait::Strafe; }
    int32 GetStrafeJumps() const { return StrafeJumps; }
    /** Tests: hold strafe mode without a key. */
    void SetStrafeHeld(bool bHeld) { bTestStrafe = bHeld; }
    /** Movement sound effects fired so far, by kind (tests): steps, jump, land, slash, roll. */
    enum class ESfx : uint8 { Step, Jump, Land, Slash, Roll, Num };
    int32 GetSfxCount(ESfx Kind) const { return SfxCounts[static_cast<int32>(Kind)]; }
    int32 GetSfxLoaded() const;
    /** Last side jump was the long (running) one. */
    bool WasLongSideJump() const { return bSideLong; }
    int32 GetOuterCorners() const { return OuterCorners; }
    int32 GetInnerCorners() const { return InnerCorners; }
    int32 GetLandingRolls() const { return LandingRolls; }
    // Knee-high ledges he walks into are mantled automatically (above his feet, cm).
    static constexpr float MantleMin = 6.f;
    static constexpr float MantleMax = 40.f;
    // Parkour tuning (user vision: "feels like you're doing something difficult
    // but it's not actually that hard"): three steps up a wall, a strong kick.
    static constexpr float WallRunRise = 45.f;     // cm climbed over the three steps
    static constexpr float WallRunTime = .45f;     // s (three 0.15 s steps)
    static constexpr float WallReach = 12.f;       // cm beyond the capsule a wall still catches him
    static constexpr float WallJumpOut = 260.f;    // cm/s away from the wall (side jump: 190)
    static constexpr float WallJumpUp = 230.f;     // cm/s up (standing jump: 170)
    static constexpr float WallCoyote = .15f;      // s after leaving a wall a jump still kicks off it
    static constexpr float WallBuffer = .15f;      // s a jump pressed before reaching a wall still counts
    /** Bitten by a rat at From: knocked back a step, then briefly safe from
     *  bites. Rolling or side-jumping dodges it; no bite reaches him on a wall.
     *  Returns whether it landed. (No health yet: the user's call.) */
    bool TakeBite(const FVector& From);
    // Sanity (the 2D game, GAME-BIBLE.md): damage lowers it, cigarettes
    // restore it; at zero Chuck - a fey summon who can't die - quietly
    // vanishes into astral light and is summoned back at the spawn point.
    static constexpr int32 MaxSanity = 5;
    int32 GetSanity() const { return Sanity; }
    /** Tests. */
    void SetSanity(int32 Value) { Sanity = FMath::Clamp(Value, 0, MaxSanity); }
    /** Vanishing, away or being summoned back. */
    bool IsAstral() const { return Gait == EGait::Astral || bPendingVanish; }
    const TCHAR* GetAstralName() const;
    int32 GetRespawns() const { return Respawns; }
    /** Every cigarette picked up (refilling Sanity or counted). */
    int32 GetPickupsCollected() const { return PickupsCollected; }
    int32 GetBitesTaken() const { return BitesTaken; }
    static constexpr float BiteKnockback = 230.f;   // cm/s away
    static constexpr float BiteImmunity = 1.f;       // s
    /** Cigarettes collected (the currency; HUD counter). */
    int32 GetCigarettes() const { return CigaretteCount; }
    /** Picked up: each refills a point of Sanity first; once it's full they count up like coins. */
    void AddCigarettes(int32 Count);
    /** Breakables (grass tufts, jars, later small rats) broken by the slash so far. */
    int32 GetSlashBreaks() const { return SlashBreaks; }
    int32 GetSlashRatHits() const { return SlashRatHits; }
    // The slash's reach for breakables: from his centre to a target's edge (cm),
    // ground to chest. Generous on purpose ("hard-looking but easy").
    static constexpr float SlashReach = 40.f;
    /** Strikes started so far (tests). */
    int32 GetSlashStrikes() const { return SlashStrikes; }
    /** Tests: a fixed paw sequence. */
    void SetSlashSeed(int32 Seed) { SlashRandom.Initialize(Seed); }
    /** "SlashRight"/"SlashLeft" while one plays (standing or layered), else "". */
    const TCHAR* GetSlashName() const;
    /** Run latch: tap Shift / LB to run, tap again or come to a stop to saunter. Tests set it directly. */
    void SetRunHeld(bool bHeld) { bRunHeld = bHeld; }
    /** Current saunter -> run blend (0..1). */
    float GetRunWeight() const { return RunWeight; }
    /** Tests (input disabled): the raw stick a dodge reads to choose its exit. */
    void SetTestStick(FVector2D Stick) { InputRight = Stick.X; InputForward = Stick.Y; }
    /** Tests that enable live input: ignore mouse/stick look so a real mouse cannot steer them. */
    void SetLookLocked(bool bLocked) { bLookLocked = bLocked; }
    /** In the air on a leap out of a run. */
    bool IsRunJumping() const { return Gait == EGait::Air && bRunJump; }
    void Recenter();
    /** True when the orbit sits in its upper (elevated) half. */
    bool IsElevated() const;
    /** Orbit look pitch (deg): -60 high, -48 elevated preset, -5 rat height, up to +30 looking up. */
    float GetLookPitch() const { return LookPitch; }
    /** Captures: set the orbit height directly. */
    void SetOrbitPitch(float Pitch) { LookPitch = SmoothLook = Pitch; }
    static FVector StartLocation() { return FVector(-240, -180, 36); }
    /** v1 animation instance on GetMesh(); null until play begins. */
    UChuckAnimInstance* GetChuckAnim() const;
    int32 GetGroomCount() const;
    /** Cigarette prop on socket_cigarette; null with -ChuckNoCigarette. */
    UStaticMeshComponent* GetCigarette() const { return Cigarette; }
    UStaticMeshComponent* GetCigaretteSmoke() const { return Smoke; }
    /** Breathe out a stream of smoke now (user 2026-09-30: he's always
     *  smoking; he exhales every 7-12 s on his own when calm). */
    void Exhale();
    int32 GetExhales() const { return Exhales; }
    int32 GetSmokePuffsSpawned() const { return SmokePuffsSpawned; }
    int32 GetSmokePuffsLive() const { return SmokePuffs.Num(); }
    static constexpr float ExhaleEvery = 7.f;      // s, plus up to 5 s more
    static constexpr float ExhaleLength = .7f;     // s of breath
    /** Current locomotion state for tests and captures: Idle, Start, Loop, Stop, Turn, Air, Land, Roll, SideJump, Slash, WallRun, Hang, Climb or Strafe. */
    const TCHAR* GetGaitName() const;
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY() USpringArmComponent* Boom;
    UPROPERTY() UCameraComponent* Camera;
    UPROPERTY() TArray<UGroomComponent*> Grooms;
    UPROPERTY() USkeletalMesh* PlainMesh;
    UPROPERTY() UStaticMeshComponent* Cigarette;
    UPROPERTY() UStaticMeshComponent* Smoke;
    UPROPERTY() UInstancedStaticMeshComponent* ExhaleSmoke;
    UPROPERTY() TArray<USoundBase*> ExhaleSounds;
    struct FSmokePuff { FVector Position; FVector Velocity; float Age; float Life; float Size; };
    TArray<FSmokePuff> SmokePuffs;
    float NextExhaleAt = 5.f;
    float ExhaleUntil = -1.f;
    float ExhaleCarry = 0.f;
    int32 Exhales = 0;
    int32 SmokePuffsSpawned = 0;
    void UpdateExhale(float DeltaSeconds);
    float ViewYaw = 0;
    /** One continuous GTA-style orbit instead of two switched cameras. */
    float LookPitch = -48;
    float SmoothLook = -48;
    /** Seconds since the last look input; the orbit drifts behind Chuck after a while. */
    float LookIdle = 0;
    /** Camera pivot height: holds through jumps, follows real level changes. */
    float FollowZ = 0;
    bool bFollowReady = false;

    // v1 clips (docs/RIG-CONTRACT-V1.md, SourceAssets/Chuck/V1/Animations/manifest.json).
    enum class EClip : uint8 { Idle, WalkStart, WalkLoop, WalkStop, TurnLeft90, TurnRight90, JumpStart, JumpLoop, JumpLand, Roll, SideJumpLeft, SideJumpRight, RunLoop, RunJump, SlashRight, SlashLeft, WallRun, WallKick, Hang, PullUp, Mantle, ShimmyLeft, ShimmyRight, StrafeLeft, StrafeRight, StrafeRunLeft, StrafeRunRight, SlashLowRight, SlashLowLeft, Summon, Num };
    UPROPERTY() TArray<UAnimSequence*> Clips;
    enum class EGait : uint8 { Idle, Start, Loop, Stop, Turn, Air, Land, Roll, SideJump, Slash, WallRun, Hang, Climb, Strafe, Astral };
    EGait Gait = EGait::Idle;
    EClip Base = EClip::Idle;
    float BaseTime = 0;
    EClip Fading = EClip::Idle;
    float FadingTime = 0;
    float FadeWeight = 0;
    float FadeRate = 0;
    float StateTime = 0;
    float StartDistance = 0;
    float WalkPhase = 0;
    float StopTravel = 0;
    bool bStopPending = false;
    bool bStopMirror = false;
    float TurnStartYaw = 0;
    float TurnDelta = 0;
    float MeshDrop = 0;
    // Dodges (roll / side jump) and the raw stick that chooses between them.
    float InputForward = 0;
    float InputRight = 0;
    FVector DodgeDirection = FVector::ForwardVector;
    bool bDodgeLaunched = false;
    bool bDodgeLanded = false;
    float RollDone = 0;  // capsule distance actually covered in this roll
    bool bRunHeld = false;
    float RunWeight = 0;
    bool bRunJump = false;
    bool bLookLocked = false;
    // Slash: the standing gait uses Base; on the move a layer plays (time < 0: none).
    bool bSlashQueued = false;
    bool bSlashHeld = false;
    FRandomStream SlashRandom;
    int32 SlashStrikes = 0;
    int32 SlashBreaks = 0;
    int32 SlashRatHits = 0;
    int32 CigaretteCount = 0;
    int32 BitesTaken = 0;
    int32 Sanity = MaxSanity;
    int32 PickupsCollected = 0;
    enum class EAstral : uint8 { None, Vanishing, Away, Summoning };
    EAstral AstralPhase = EAstral::None;
    float AstralClock = 0;
    bool bPendingVanish = false;
    float PendingVanishAt = 0;
    bool bAstralHidden = false;
    bool bAstralFaded = false;
    int32 Respawns = 0;
    void BeginVanish();
    void UpdateAstral(float DeltaSeconds);
    void SetAstralHidden(bool bHide);
    void CameraFade(float From, float To, float Seconds);
    float BiteImmuneUntil = -1;
    /** Something low and breakable is in the strike's reach: rake low (user 2026-09-30). */
    bool LowTargetInReach() const;
    float SlashHitAt = -1;   // world time of the pending strike's cut
    void SlashHit();
    int32 SamePawRun = 0;
    EClip PickPaw(bool bFirst, EClip Previous);
    float SlashDone = 0;
    EClip LayerClip = EClip::SlashRight;
    float LayerTime = -1;
    EClip FadingLayerClip = EClip::SlashRight;
    float FadingLayerTime = -1;
    float FadingLayerWeight = 0;
    bool OwnsCapsule() const { return IsDodging() || Gait == EGait::Astral || Gait == EGait::Slash || Gait == EGait::WallRun || Gait == EGait::Hang || Gait == EGait::Climb; }
    // Ledge state.
    FVector HangNormal = FVector::ZeroVector;
    FVector HangEdge = FVector::ZeroVector;   // the top edge on the wall face
    FVector HangFrom = FVector::ZeroVector;   // snap-in start
    float HangClock = 0;
    float HangHold = 0;
    float ShimmyPhase = 0;
    float HangSnapTime = .12f;     // snap-in (grab) or turn (corner) duration
    float HangYawFrom = 0;
    // Round a corner the stick you were shimmying with keeps meaning "carry on
    // this way" (the camera lags the turn), until you move the stick.
    bool bCornerCarry = false;
    float CornerCarrySide = 0;
    FVector2D CornerCarryStick = FVector2D::ZeroVector;
    int32 OuterCorners = 0;
    int32 InnerCorners = 0;
    float AirApexZ = 0;            // highest point of the current fall
    int32 LandingRolls = 0;
    // Strafe state.
    float StrafeKeys = 0;          // Q/E axis
    float StrafeTrigger = 0;       // LT axis
    bool bTestStrafe = false;
    float StrafePhase = 0;
    int32 StrafeJumps = 0;
    bool bSideLong = false;
    bool StrafeHeld() const { return StrafeKeys != 0 || StrafeTrigger > .3f || bTestStrafe; }
    /** Sideways stick including Q/E. */
    float SideInput() const { return FMath::Clamp(InputRight + StrafeKeys, -1.f, 1.f); }
    void StrafeKeysAxis(float Value);
    void StrafeTriggerAxis(float Value) { StrafeTrigger = Value; }
    // Drop to hang: the stick that walked him off is ignored until released.
    bool bHangNeedsRelease = false;
    int32 DropHangs = 0;
    bool TryDropHang();
    // Movement SFX (SourceAssets/Audio/SFX, Tools/gen_chuck_sfx.py), 2D under the soundtrack.
    UPROPERTY() TArray<USoundBase*> StepWalkWood;
    UPROPERTY() TArray<USoundBase*> StepWalkStone;
    UPROPERTY() TArray<USoundBase*> StepRunWood;
    UPROPERTY() TArray<USoundBase*> StepRunStone;
    UPROPERTY() TArray<USoundBase*> JumpSounds;
    UPROPERTY() TArray<USoundBase*> LandSounds;
    UPROPERTY() TArray<USoundBase*> SlashSounds;
    UPROPERTY() TArray<USoundBase*> RollSounds;
    int32 SfxCounts[static_cast<int32>(ESfx::Num)] = {};
    bool bPrevStance[2] = {false, false};
    void PlaySfx(const TArray<USoundBase*>& Set, ESfx Kind, float Volume, float StartTime = 0.f);
    void PlayStep(const FVector& Paw, float Speed);
    bool TryHangCorner(const FVector& Along, float Side);
    void TurnHangCorner(const FVector& Normal, const FVector& Edge, bool bRoom, float Side);
    void LandingRoll();
    bool bHangRoom = false;
    FVector ClimbStart = FVector::ZeroVector;
    FVector ClimbDir = FVector::ZeroVector;
    float ClimbRise = 0;
    float ClimbAdvance = 0;
    bool bClimbMantle = false;
    float LedgeCooldownUntil = -1;
    int32 Hangs = 0;
    int32 PullUps = 0;
    int32 Mantles = 0;
    bool FindLedge(const FVector& Normal, const FVector& FacePoint, float MinAbove, float MaxAbove, FVector& OutEdge, bool& bRoom) const;
    void EnterHang(const FVector& Normal, const FVector& Edge, bool bRoom);
    void DropFromHang();
    void StartClimb(bool bMantle, const FVector& Normal, const FVector& Edge);
    bool TryMantle();
    // Wall run / wall jump state.
    FVector WallNormal = FVector::ZeroVector;      // horizontal, out of the wall
    FVector LastWallNormal = FVector::ZeroVector;  // the wall last run: no fresh steps until another wall or the ground
    float WallRunClock = 0;
    float WallPhase = 0;
    float WallPrevZ = 0;
    bool bWallAuto = false;        // caught after a wall jump: the stick isn't needed
    bool bWallJumpFlight = false;  // in the air from a wall jump
    bool bChimney = false;         // another wall faces this one behind him: frame it side-on
    float WallCoyoteUntil = -1;
    float AirJumpPressedAt = -1e3f;
    int32 WallRuns = 0;
    int32 WallJumps = 0;
    FVector StickWorld() const;
    bool TryEnterWallRun();
    void EnterWallRun(const FHitResult& Hit, const FVector& Normal);
    void LeaveWall();
    void WallJump();
    void UpdateSlashLayer(float DeltaSeconds);
    bool bHardLanding = false;
    // A latch, not a hold: holding Shift while pressing Space (and a direction)
    // can exceed what many keyboards register at once.
    void RunPressed() { bRunHeld = !bRunHeld; }
    bool IsDodging() const { return Gait == EGait::Roll || Gait == EGait::SideJump; }
    void Dodge();
    void FinishDodge();
    FVector PreviousMotionLocation = FVector::ZeroVector;
    void SetClip(EClip Clip, float Time, float FadeSeconds);
    float Period(EClip Clip) const;
    float FindGround(const FVector& Near, float Fallback) const;

    void Forward(float Value);
    void Right(float Value);
    void MouseLook(float Value);
    void Turn(float Value);
    void MousePitch(float Value);
    void StickPitch(float Value);
    void Quit();
    void UpdateCamera(float DeltaSeconds = 0);
    void UpdateMotion(float DeltaSeconds);
};
