#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FeyCreatures.generated.h"

class UStaticMeshComponent;
class USoundBase;
class USoundAttenuation;
class AChuckCharacter;

// The Feywild's creatures, stage 5 part two (Claude, 2026-11-22; docs/regions/FEYWILD.md, after the 2D game's PHASE-9.md
// sections 7.1 and 7.5). Meshes by Tools/build_fey_creatures.py; the redcaps are temple skeletons of their own kind
// (TempleSkeleton.h, Post.Kind 3) on that rig's clips. Placed and reset by AFeyLife, one district at a time.
//
//  - AFeyMite: a thorn mite, smaller than Chuck. It noses about its home, notices him close by (short notice), scuttles
//    straight at him, crouches and hisses (the tell), then lunges and bites for a Sanity point; one committed scratch
//    kills it. Small groups.
//  - AFeyDisplacer: the displacer beast, a great six-legged panther with two tentacles from its shoulders, ending in
//    spiked pads. It prowls a round far from where he arrives and notices him from far off; it roars, then runs him down
//    and lashes with its tentacles (two Sanity points and a knock back). It cannot enter the Chuck-sized refuges (the
//    hollow logs): it paces outside, growling, and gives up. Like the D&D beast, it is never quite where it appears: its
//    image wavers a little to one side of where it truly is. Technically it can be scratched (thirty scratches drive
//    it off), but it is meant to be avoided.

/** A circle he can shelter in that the beast cannot enter (local X, Y, radius). */
struct FFeyRefuge { FVector2D C = FVector2D::ZeroVector; float R = 150.f; };

UCLASS()
class CHUCK3D_API AFeyMite : public AActor
{
    GENERATED_BODY()
public:
    AFeyMite();
    virtual void Tick(float DeltaSeconds) override;
    enum class EState : uint8 { Roam, Chase, Windup, Lunge, Recover, Dead };
    /** Home on the ground (local), facing Yaw. */
    void Init(const FVector& HomeLocal, float Yaw);
    /** Alive again at home (his respawn). */
    void ResetMite();
    /** His scratch: true if it struck this mite (and killed it). */
    bool TakeScratch(const FVector& Center, float Radius);
    bool IsDead() const { return State == EState::Dead; }
    EState GetState() const { return State; }
    FVector GetLocal() const { return Pos; }
    static const TArray<TWeakObjectPtr<AFeyMite>>& All();
    static int32 Bites;
    static int32 Kills;
    static constexpr float NoticeRange = 380.f;
    static constexpr float LoseRange = 900.f;
    static constexpr float ChaseSpeed = 150.f;
    static constexpr float RoamSpeed = 35.f;
    static constexpr float StrikeRange = 55.f;
    static constexpr float WindupTime = .38f;
    static constexpr float LungeTime = .22f;
    static constexpr float LungeSpeed = 330.f;
    static constexpr float BiteRange = 42.f;
    static constexpr float RecoverTime = .65f;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() USceneComponent* Pivot = nullptr;
    UPROPERTY() UStaticMeshComponent* Body = nullptr;
    UPROPERTY() TArray<UStaticMeshComponent*> Legs;
    UPROPERTY() USoundAttenuation* Attenuation = nullptr;
    UPROPERTY() TArray<USoundBase*> Sounds;   // chitter, tell, bite, death (each a set: SoundSets)
    TArray<FIntPoint> SoundSets;               // (first, count) in Sounds
    FVector Home = FVector::ZeroVector, Pos = FVector::ZeroVector, Wander = FVector::ZeroVector, LungeDir = FVector::ForwardVector;
    float Yaw = 0, HomeYaw = 0, StateTime = 0, Gait = 0, ChitterAt = 0, DeadTime = 0;
    bool bBit = false;
    EState State = EState::Roam;
    FRandomStream Rand;
    void SetState(EState S);
    void Sound(int32 Which, float Volume, float Pitch);
    bool MoveToward(const FVector& Dir, float Speed, float Dt);
    void Pose(float Dt);
};

UCLASS()
class CHUCK3D_API AFeyDisplacer : public AActor
{
    GENERATED_BODY()
public:
    AFeyDisplacer();
    virtual void Tick(float DeltaSeconds) override;
    enum class EState : uint8 { Prowl, Notice, Chase, Lash, Recover, Pace, Return, Flinch, Driven };
    /** Its round (local, on the ground), walked in a loop; the refuges it cannot enter. */
    void Init(const TArray<FVector>& Round, const TArray<FFeyRefuge>& InRefuges);
    void ResetBeast();
    bool TakeScratch(const FVector& Center, float Radius);
    EState GetState() const { return State; }
    const TCHAR* GetStateName() const;
    FVector GetLocal() const { return Pos; }
    /** Where its image is drawn (local), a little to the side of where it is. */
    FVector GetImageLocal() const;
    bool InRefuge(const FVector& Local, float Margin = 0.f) const;
    static const TArray<TWeakObjectPtr<AFeyDisplacer>>& All();
    static int32 Lashes;
    static int32 LashHits;
    static int32 Notices;
    static int32 Paces;
    static constexpr float NoticeRange = 2600.f;
    static constexpr float LoseRange = 5200.f;
    static constexpr float ProwlSpeed = 110.f;
    static constexpr float ChaseSpeed = 250.f;
    static constexpr float LashReach = 330.f;
    static constexpr float HipHeight = 112.f;
    static constexpr float Health = 30.f;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    struct FLeg { FVector HipLocal, Foot, From, To; float Lift = 1.f; bool bFront = true; UStaticMeshComponent* Upper = nullptr; UStaticMeshComponent* Lower = nullptr; };
    UPROPERTY() USceneComponent* Image = nullptr;      // the body as drawn (displaced)
    UPROPERTY() UStaticMeshComponent* Body = nullptr;
    UPROPERTY() UStaticMeshComponent* Head = nullptr;
    UPROPERTY() UStaticMeshComponent* Tail = nullptr;
    UPROPERTY() TArray<UStaticMeshComponent*> Parts;   // every leg segment, tentacle segment and pad (kept for GC)
    UPROPERTY() USoundAttenuation* Attenuation = nullptr;
    UPROPERTY() TArray<USoundBase*> Sounds;           // step, growl, roar, lash, hit (each a set: SoundSets)
    TArray<FIntPoint> SoundSets;
    UPROPERTY() TArray<class UMaterialInstanceDynamic*> FurMids;   // every part's, for the shimmer as its image slips
    float Shimmer = 0;
    TArray<FLeg> LegsState;
    TArray<UStaticMeshComponent*> Tentacles[2];
    TArray<FVector> Round;
    TArray<FFeyRefuge> Refuges;
    FVector Pos = FVector::ZeroVector, Vel = FVector::ZeroVector, LashAt = FVector::ZeroVector;
    float Yaw = 0, StateTime = 0, Clock = 0, Hits = 0, Lost = 0, GrowlAt = 0, StepGroupTime = 0, BodyZ = 0, BodyPitch = 0, Lash = 0;
    int32 RoundAt = 0, StepGroup = 0;
    bool bLashLanded = false;
    EState State = EState::Prowl;
    FRandomStream Rand;
    void SetState(EState S);
    void Sound(int32 Which, float Volume, float Pitch);
    void Walk(const FVector& Goal, float Speed, float TurnRate, float Dt);
    void Pose(float Dt);
    float GroundZ(float X, float Y) const;
};

/** The mites and the beast in reach of his scratch (FeyScratch). */
bool FeyCreaturesScratch(const FVector& World, const FVector& Ahead, float Reach);
