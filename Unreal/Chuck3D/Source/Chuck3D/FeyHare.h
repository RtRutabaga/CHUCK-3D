#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FeyHare.generated.h"

class UPoseableMeshComponent;
class UMaterialInstanceDynamic;
class USoundBase;
class USoundAttenuation;
class AChuckCharacter;

// The Feywild's great hare (Claude, 2026-11-25; docs/regions/FEYWILD-HARE.md, approved with its recommendations). A
// rideable mount for the Feywild only: herds of two in Highmeadow (two herds), the Crossroads, the orchard and the Lily
// Mere, spawned and reset by AFeyLife with their district. Mesh by Tools/build_fey_hare.py, posed here in code (a
// procedural gait on a poseable mesh: foot paths and two-bone IK, the back flexing with the bound).
//
//  - Unridden it grazes, sits up with its ears turning, hops a few metres and settles, within Leash of its home. Run or
//    sprint straight at it from close and it lollops off; walk up to it and it lets him near. Within MountReach of a calm
//    hare the talk key (F / Y) shows "Ride" and puts him on its back.
//  - Ridden, the stick steers it (camera-relative, as his walking): a light push a hop-along (HopSpeed), Run the bound
//    (BoundSpeed; the displacer beasts chase at 250), Jump a long leap, Roll or the talk key again slides him off and it
//    stays, calm. Pollen halves it.
//  - It keeps to its own district's open ground: about 8 m from a threshold's mouth it slows and turns aside, and
//    pressing on makes it thump a hind foot and turn away; so too at Highmeadow's western brink (the overlook's drop to the
//    maze) and at deep water (a stream it wades). (2026-11-26, the user: bucks only where the game needs them) It rears
//    and throws him only if it is on its way out of its district all the same (on a threshold's way, off its land), then
//    bolts home; he lands in his CrashLand, no Sanity lost. Bites, seeds and moths do not throw him. Nothing saved: a
//    death or a checkpoint start puts every hare back at home, unridden.
UCLASS()
class CHUCK3D_API AFeyHare : public ACharacter
{
    GENERATED_BODY()
public:
    AFeyHare(const FObjectInitializer& ObjectInitializer);
    virtual void Tick(float DeltaSeconds) override;
    enum class EState : uint8 { Graze, Alert, Hop, Shy, Ridden, Refuse, Rear, Bolt };
    /** Its home (world, on the ground), the district it keeps to, which of the herd (its coat), facing Yaw. */
    void Init(int32 InDistrict, const FVector& HomeWorld, float Yaw, int32 Index);
    /** Home again, unridden (his respawn, a checkpoint start). */
    void ResetHare();
    /** The calm hare within reach of him, if any (the prompt; the talk key mounts it). */
    static AFeyHare* MountPrompt(const AChuckCharacter* Chuck);
    bool CanMount(const AChuckCharacter* Chuck) const;
    bool Mount(AChuckCharacter* Chuck);
    /** He slides off (Roll or the talk key): the hare stays where it is, calm. */
    void Dismount();
    /** The leap (his jump while riding). */
    void Leap();
    /** Rears and throws him (Why, for the log). */
    void Throw(const TCHAR* Why);
    /** For him while he rides: his seat (world) and its turn (the hare's yaw with its body's pitch and lean). */
    FVector GetSeat() const { return Seat; }
    FQuat GetSeatQuat() const { return SeatQuat; }
    /** The bound's phase (0-1) and how far into the bound it is (0 at a standstill, 1 at full speed). */
    float GetPhase() const { return Phase; }
    float GetBoundWeight() const { return BoundW; }
    FVector GetRideVelocity() const { return GetVelocity(); }
    EState GetState() const { return State; }
    const TCHAR* GetStateName() const;
    int32 GetDistrict() const { return District; }
    FVector GetHome() const { return Home; }
    AChuckCharacter* GetRider() const { return Rider.Get(); }
    static const TArray<TWeakObjectPtr<AFeyHare>>& All();
    // Counters for the tests (FeyMapTest's life steps).
    static int32 Mounts;
    static int32 Dismounts;
    static int32 Throws;
    static int32 Refusals;
    static int32 Thumps;
    static int32 Shies;
    static int32 Leaps;
    static int32 Bolts;
    static float TopSpeed;   // the fastest it has been ridden (cm/s), on the ground
    static FString LastThrow;
    /** Tests: pretend he pushes the stick along Dir (world, 2D; zero to let go) with Run. */
    static FVector TestStick;
    static bool bTestStick;
    static bool bTestRun;
    /** Stills (FeyMapTest's pose review): hare 0 held in State at Speed (cm/s), its bound at Phase, in the air or not. */
    static bool bPoseHold;
    static float PoseSpeed;
    static float PosePhase;
    static bool bPoseAir;
    static EState PoseState;
    static constexpr float HopSpeed = 150.f;
    static constexpr float BoundSpeed = 550.f;
    static constexpr float MountReach = 120.f;
    static constexpr float Leash = 2500.f;
    static constexpr float ShyRange = 600.f;
    static constexpr float RefuseRange = 800.f;    // from a threshold's mouth
    static constexpr float BrinkRange = 1700.f;    // from Highmeadow's western edge (the overlook's drop)
    static constexpr float HalfHeight = 40.f;
    static constexpr float Radius = 30.f;
    static constexpr float SeatZ = 7.5f;          // his mesh's origin below the saddle (Tools/build_chuck_v1.py RIDE_SEAT_Z)
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() UPoseableMeshComponent* Body = nullptr;
    UPROPERTY() UMaterialInstanceDynamic* Coat = nullptr;
    UPROPERTY() USoundAttenuation* Attenuation = nullptr;
    UPROPERTY() TArray<USoundBase*> Sounds;   // thump, bound, squeal (sets: SoundSets)
    TArray<FIntPoint> SoundSets;
    TWeakObjectPtr<AChuckCharacter> Rider;
    int32 District = -1, Index = 0;
    EState State = EState::Graze;
    FVector Home = FVector::ZeroVector, Goal = FVector::ZeroVector, Seat = FVector::ZeroVector;
    FQuat SeatQuat = FQuat::Identity;
    float HomeYaw = 0, StateTime = 0, StateLen = 3, Phase = 0, BoundW = 0, AirW = 0, RefuseTime = 0, TurnAway = 0, Slope = 0, Lean = 0, LastYaw = 0, ThumpAt = -1, BoundSoundPhase = 0;
    float Crouch = 0, GrazeW = 0, SitW = 0, RearW = 0, EarBack = 0, EarTurn = 0, EarTurnGoal = 0, NextEarTurn = 0, ScutUp = 0, Clock = 0, CalmUntil = 0;
    bool bLeaping = false;
    FRandomStream Rand;
    // The skeleton (cached at BeginPlay): rest transforms, parents, the joints by role.
    TArray<FTransform> Local;
    TArray<int32> Parent;
    TArray<FTransform> RestT;   // component space, at rest
    TArray<FVector> RestHead, RestDir, Head;   // each joint at rest, toward its child at rest, and posed (component space)
    TArray<FQuat> Turned;       // each bone's turn from rest (component space), about its own joint
    TArray<float> BoneLen;        // each bone's length to its child (component space, at rest)
    TArray<FVector> ChildDir;   // in each bone's own frame, toward its child's head (its length)
    TArray<FName> Names;
    TArray<FTransform> Space;
    enum EBone { BSpine, BChest, BNeck, BHead, BHips, BScut, BSaddle, BEar1L, BEar2L, BEar1R, BEar2R,
                 BFUpL, BFLoL, BFPawL, BFUpR, BFLoR, BFPawR, BThighL, BShinL, BFootL, BToeL, BThighR, BShinR, BFootR, BToeR, BCount };
    int32 Bone[BCount];
    bool bRig = false;
    FVector FootGoal[4];   // fore L, fore R, hind L, hind R (component space)
    float HeelLift[4];
    float LegY[4] = {-8.5f, 8.5f, -11.5f, 11.5f};   // (each leg's side, from the skeleton's rest: the import mirrors Blender's)
    void SetState(EState S, float Len = 0.f);
    void Sound(int32 Which, float Volume, float Pitch = 1.f);
    void Think(float Dt);
    void Ride(float Dt);
    bool PickGoal(const FVector& Near, float Radius, FVector& Out);
    bool OpenGround(const FVector& World) const;
    bool InThrowZone(const FVector& World) const;
    bool RefuseDir(const FVector& World, FVector& Dir) const;
    void Steer(const FVector& Dir, float Speed);
    void Pose(float Dt);
    void Aim(int32 B, const FVector& Dir);
    void ReleaseRider(bool bThrow);
};
