#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TempleBeholder.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UPoseableMeshComponent;
class USkeletalMesh;
class ATempleSanctumBattle;

/**
 * The beholder of the sanctum (user 2026-10-23: "start creating the beholder, it should look really cool
 * and detailed"; docs/CHULT-TEMPLE-BEHOLDER.md). SK_Beholder (Tools/build_beholder.py) hangs over the
 * front of the dais where the heroes already look, alive but not yet fighting: it hovers and bobs, its
 * ten stalks writhe and their eyes each watch someone, the great eye picks out a hero (or Chuck, when he
 * is near and in front of it) with quick saccades, the lids blink and follow its gaze, the jaw breathes
 * and now and then draws back in a snarl, and the iris glows. Posed in component space by a small
 * forward pass over the rig: each bone's own turn is set in its rest frame and carried by its parent's.
 */
UCLASS()
class CHUCK3D_API ATempleBeholder : public AActor
{
    GENERATED_BODY()
public:
    ATempleBeholder();
    virtual void Tick(float DeltaSeconds) override;
    void Setup(USkeletalMesh* Mesh, ATempleSanctumBattle* InBattle, const FVector& Home);

    FVector GetEyeLocation() const;
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
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY() UPoseableMeshComponent* Body = nullptr;
    UPROPERTY() UPointLightComponent* EyeLight = nullptr;
    UPROPERTY() UMaterialInstanceDynamic* EyeMat = nullptr;
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
    struct FStalk { float Phase1 = 0, Phase2 = 0, Rate1 = .4f, Rate2 = .5f, NextLook = 0; FVector Look = FVector::ZeroVector; FQuat Aim = FQuat::Identity; int32 Eye = INDEX_NONE; FVector RestEye = FVector::ZeroVector; };
    TArray<FStalk> Stalks;
    FVector Home = FVector::ZeroVector, EyeRest = FVector::ZeroVector, HeldGaze = FVector::ZeroVector;
    FQuat EyeAim = FQuat::Identity;
    FVector GazeAt = FVector::ZeroVector;
    float T = 0, NextGaze = 0, NextBlink = 2.f, BlinkStart = -10.f, NextSnarl = 6.f, SnarlStart = -10.f, SnarlLen = 1.f, SnarlOpen = 18.f;
    float JawNow = 0, Yaw = 0, StalkTravel = 0, CloseSign = 1.f;
    int32 Poses = 0, Blinks = 0, Snarls = 0, Saccades = 0;
    FRandomStream Random;

    void ReadRig();
    void PickGaze();
    FVector Watchable(int32 Pick) const;
    void Pose(float Dt);
};
