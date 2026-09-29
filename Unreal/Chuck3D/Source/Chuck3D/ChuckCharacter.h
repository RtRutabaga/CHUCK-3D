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
    static FVector StartLocation() { return FVector(-240, -180, 36); }
    /** v1 animation instance on GetMesh(); null until play begins. */
    UChuckAnimInstance* GetChuckAnim() const;
    int32 GetGroomCount() const;
    /** Cigarette prop on socket_cigarette; null with -ChuckNoCigarette. */
    UStaticMeshComponent* GetCigarette() const { return Cigarette; }
    UStaticMeshComponent* GetCigaretteSmoke() const { return Smoke; }
    /** Current locomotion state for tests and captures: Idle, Start, Loop, Stop, Turn, Air, Land, Roll, SideJump or Slash. */
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
    enum class EClip : uint8 { Idle, WalkStart, WalkLoop, WalkStop, TurnLeft90, TurnRight90, JumpStart, JumpLoop, JumpLand, Roll, SideJumpLeft, SideJumpRight, RunLoop, RunJump, SlashRight, SlashLeft, Num };
    UPROPERTY() TArray<UAnimSequence*> Clips;
    enum class EGait : uint8 { Idle, Start, Loop, Stop, Turn, Air, Land, Roll, SideJump, Slash };
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
    int32 SamePawRun = 0;
    EClip PickPaw(bool bFirst, EClip Previous);
    float SlashDone = 0;
    EClip LayerClip = EClip::SlashRight;
    float LayerTime = -1;
    EClip FadingLayerClip = EClip::SlashRight;
    float FadingLayerTime = -1;
    float FadingLayerWeight = 0;
    bool OwnsCapsule() const { return IsDodging() || Gait == EGait::Slash; }
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
