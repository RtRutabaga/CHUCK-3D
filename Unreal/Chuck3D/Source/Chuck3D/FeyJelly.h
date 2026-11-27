#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FeyJelly.generated.h"

class UProceduralMeshComponent;
class UPointLightComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
class USoundBase;
class USoundAttenuation;
class AChuckCharacter;

// The Deepwood's jellyfish (Claude, 2026-11-26; docs/regions/FEYWILD-JELLYFISH.md, approved by the user with its
// recommendations): a mount he hangs from by two of its oral arms. Spawned and reset by AFeyLife with the Deepwood.
//
//  - Unridden it drifts slowly about its home, pulsing, its arms hanging to his raised paws. Nothing about him troubles
//    it. Within Reach under it the talk key (F / Y) shows "Hold on" and he takes hold; it rises, lifting him.
//  - Held, the stick steers it, camera-relative, up to Speed (5 m/s) by how far it is pushed: no run toggle, no up or
//    down. It follows the ground with his feet about 1.2 m up, eased over roots and hills, and sinks under low arches
//    (never below 30 cm over his feet; where even that will not pass it will not go). Trunks turn it aside. He hangs
//    under it as a pendulum (the swing is solved here; he reads it). Roll or the talk key lets go: he drops to his feet.
//  - It keeps to the Deepwood: near a threshold's mouth, Redcap Hollow's clearing or a crossing's barrier (Barriers(),
//    added by whoever builds a crossing) it drifts back. Only if it is on its way out of the district all the same does
//    it shake him off. Bites do not. A death or a checkpoint start sends every one home.
//  - Drawn here: the bell (a procedural mesh, pulsing), two pairs of frilled oral arms and twelve fine tendrils (simulated
//    strands), a small unshadowed light. M_FeyJelly (Tools/create_fey_jelly_assets.py).
UCLASS()
class CHUCK3D_API AFeyJelly : public AActor
{
    GENERATED_BODY()
public:
    AFeyJelly();
    virtual void Tick(float DeltaSeconds) override;
    enum class EState : uint8 { Drift, Rise, Held, Wait };
    /** Its home (world, on the ground), the district it keeps to, which one it is (its tint), the stretch it keeps to
     *  (-1: any; between crossings once there are some). */
    void Init(int32 InDistrict, const FVector& HomeWorld, int32 InIndex);
    void ResetJelly();
    /** The jellyfish he could take hold of now, if any (the prompt; the talk key takes hold). */
    static AFeyJelly* HoldPrompt(const AChuckCharacter* Chuck);
    bool CanHold(const AChuckCharacter* Chuck) const;
    bool Hold(AChuckCharacter* Chuck);
    /** He lets go (bShaken: it shook him off, on its way out of the district). */
    void Release(bool bShaken);
    /** For him while he hangs: the point between his paws (world), the swing's turn (the arms and he swing as one from
     *  under the bell), the way he faces, and how far forward he has swung (degrees, + feet ahead). */
    FVector GetGrip() const { return Grip; }
    FQuat GetSwingQuat() const { return SwingQ; }
    float GetFacingYaw() const { return FacingYaw; }
    float GetSwingForwardDeg() const;
    float GetLift() const { return Lift; }
    FVector GetDriftVelocity() const { return Vel; }
    float GetSpeedFraction() const { return static_cast<float>(Vel.Size2D()) / Speed; }
    EState GetState() const { return State; }
    const TCHAR* GetStateName() const;
    int32 GetDistrict() const { return District; }
    FVector GetHome() const { return Home; }
    AChuckCharacter* GetRider() const { return Rider.Get(); }
    /** His feet's clearance over the ground under them now (cm; while held). */
    float GetFeetClearance() const { return FeetClear; }
    static const TArray<TWeakObjectPtr<AFeyJelly>>& All();
    /** A crossing's barrier (the Deepwood's local coordinates, a line from wall to wall): it may not pass either way; two
     *  wait at each of Sides (either side of the crossing, where he comes down). FeyDeepCrossing.h fills it. */
    struct FBarrier { TArray<FVector2D> Line; FVector2D Sides[2]; };
    static TArray<FBarrier>& Barriers();
    // Counters for the tests.
    static int32 Holds;
    static int32 Releases;
    static int32 Shakes;
    static int32 Refusals;
    static int32 Blocks;      // stopped by too low a passage ahead
    static int32 Dips;        // sank under something overhead
    static float TopSpeed;
    /** Tests: pretend he pushes the stick along Dir (world, 2D; length = push). */
    static FVector TestStick;
    static bool bTestStick;
    static constexpr float Speed = 500.f;
    static constexpr float DriftSpeed = 50.f;
    static constexpr float Leash = 1800.f;
    static constexpr float Reach = 110.f;
    static constexpr float BellR = 58.f;
    static constexpr float BellH = 48.f;
    static constexpr float ArmDrop = 72.f;       // the bell's rim down to his paws
    static constexpr float RideGripUp = 185.f;   // his paws over the ground, held (his feet about 1.2 m up)
    static constexpr float IdleGripUp = 80.f;    // the arms' held point over the ground, unridden (his raised paws)
    static constexpr float PawsUp = 63.5f;       // his paws over his feet hanging (ChuckClipData::SwingGripZ)
    static constexpr float MinFeet = 30.f;       // the least room under his feet it will sink to
    static constexpr float RefuseRange = 800.f;
    static constexpr float HollowKeep = 2600.f;  // from Redcap Hollow's fire pit
    static constexpr float Radius = 40.f;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() USceneComponent* Root = nullptr;
    UPROPERTY() UProceduralMeshComponent* Mesh = nullptr;
    UPROPERTY() UPointLightComponent* Light = nullptr;
    UPROPERTY() UAudioComponent* Hum = nullptr;
    UPROPERTY() UMaterialInstanceDynamic* Mat = nullptr;
    UPROPERTY() USoundAttenuation* Attenuation = nullptr;
    UPROPERTY() TArray<USoundBase*> Pulses;
    TWeakObjectPtr<AChuckCharacter> Rider;
    int32 District = -1, Index = 0;
    EState State = EState::Drift;
    FVector Home = FVector::ZeroVector, Goal = FVector::ZeroVector, Grip = FVector::ZeroVector, Vel = FVector::ZeroVector, Accel = FVector::ZeroVector;
    FVector2D Swing = FVector2D::ZeroVector, SwingRate = FVector2D::ZeroVector;   // the hanging mass's displacement (rad, world XY)
    FQuat SwingQ = FQuat::Identity;
    float FacingYaw = 0, StateTime = 0, Clock = 0, Phase = 0, Lift = 0, GroundZ = 0, FeetClear = 0, RefuseTime = 0, LastPulse = 0, NextGoal = 0, Wobble = 0;
    bool bRefusing = false, bMeshBuilt = false;
    FRandomStream Rand;
    struct FStrand { TArray<FVector> X, Prev; float Seg = 10.f, R0 = 1.f, R1 = .3f; FVector RootLocal; bool bArm = false; int32 Held = -1; FColor Color; };
    TArray<FStrand> Strands;
    TArray<FColor> BellColors, StrandColors;
    void SetState(EState S) { State = S; StateTime = 0; }
    void Think(float Dt, FVector& Want);
    void Ride(float Dt, FVector& Want);
    void Move(float Dt, const FVector& Want, float MaxSpeed, float Accel);
    bool RefuseDir(const FVector& At, FVector& Dir) const;
    bool InShakeZone(const FVector& At) const;
    bool Heights(const FVector& At, float& Ground, float& MaxTop) const;   // the ground under him and the most the bell's top may rise to
    bool PickGoal(FVector& Out);
    void UpdateSwing(float Dt);
    void BuildStrands();
    void SimStrands(float Dt);
    void DrawBell(bool bCreate);
    void DrawStrands(bool bCreate);
};
