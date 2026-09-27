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

UCLASS()
class CHUCK3D_API AChuckCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AChuckCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    void ToggleCamera();
    void ResetToDock();
    void Recenter();
    bool IsElevated() const { return bElevated; }
    static FVector StartLocation() { return FVector(-240, -180, 36); }
    /** v1 animation instance on GetMesh(); null until play begins. */
    UChuckAnimInstance* GetChuckAnim() const;
    int32 GetGroomCount() const;
    /** Current locomotion state for tests and captures: Idle, Start, Loop, Stop, Turn, Air or Land. */
    const TCHAR* GetGaitName() const;
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY() USpringArmComponent* Boom;
    UPROPERTY() UCameraComponent* Camera;
    UPROPERTY() TArray<UGroomComponent*> Grooms;
    UPROPERTY() USkeletalMesh* PlainMesh;
    bool bElevated = true;
    float ViewYaw = 0;
    float ViewPitch = 0;
    float CameraBlend = 1;

    // v1 clips (docs/RIG-CONTRACT-V1.md, SourceAssets/Chuck/V1/Animations/manifest.json).
    enum class EClip : uint8 { Idle, WalkStart, WalkLoop, WalkStop, TurnLeft90, TurnRight90, JumpStart, JumpLoop, JumpLand, Num };
    UPROPERTY() TArray<UAnimSequence*> Clips;
    enum class EGait : uint8 { Idle, Start, Loop, Stop, Turn, Air, Land };
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
