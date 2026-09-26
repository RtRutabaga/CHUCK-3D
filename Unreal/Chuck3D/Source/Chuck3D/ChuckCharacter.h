#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ChuckCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;

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
protected:
    virtual void BeginPlay() override;
    virtual void Landed(const FHitResult& Hit) override;
private:
    UPROPERTY() USpringArmComponent* Boom;
    UPROPERTY() UCameraComponent* Camera;
    UPROPERTY() USceneComponent* RatVisual;
    bool bElevated = true;
    float ViewYaw = 0;
    float ViewPitch = 0;
    float CameraBlend = 1;
    float GaitPhase = 0;
    float MotionAmount = 0;
    float AirAmount = 0;
    float LandingCompression = 0;
    UPROPERTY() UStaticMeshComponent* Body;
    UPROPERTY() UStaticMeshComponent* LeftFoot;
    UPROPERTY() UStaticMeshComponent* RightFoot;

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
