#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChultTempleTraps.generated.h"

class UProceduralMeshComponent;
class UPointLightComponent;
class UMaterialInterface;
class USoundBase;
class USoundAttenuation;
class AChuckCharacter;

/**
 * The temple interior's traps (Claude, 2026-10-20; docs/CHULT-TEMPLE-INTERIOR.md),
 * after the 2D temple's spike bands, dart walls and collapsing floors:
 *  - Spike bands: iron spikes thrust up through a grate across a corridor on a
 *    3 s cycle (a rattle first), each band on its own phase. One Sanity and a
 *    knock back out of the band; a dodge roll does not get through them.
 *  - Dart launchers: a serpent mouth in the wall clicks, then fires two darts
 *    straight across the gallery. One Sanity each; a dodge roll goes through.
 *  - Cracked slabs over the Astral rift: each falls SlabDelay after he first
 *    stands on it and comes back SlabReturn later once he is clear.
 * Also flickers the interior's torches. World coordinates throughout.
 */
UCLASS()
class CHUCK3D_API AChultTempleTraps : public AActor
{
    GENERATED_BODY()
public:
    AChultTempleTraps();
    virtual void Tick(float DeltaSeconds) override;
    static AChultTempleTraps* Find(UWorld* World);

    // ---- Spikes.
    static constexpr float SpikePeriod = 3.f;
    static constexpr float SpikeTall = 48.f;       // cm: the tips' height when thrust up
    static constexpr float SpikeRest = 7.f;        // cm: the points showing at rest
    static constexpr float SpikeDangerTip = 12.f;  // cm: above this they hurt
    struct FSpikeBand
    {
        TWeakObjectPtr<UProceduralMeshComponent> Mesh;
        FVector Floor = FVector::ZeroVector;   // the band's centre on the floor
        FVector Along = FVector::ForwardVector;// down the corridor
        float HalfDepth = 80, HalfWidth = 200, Phase = 0;
        FVector Base = FVector::ZeroVector;    // the mesh's relative location with its tips fully up
        int32 Stage = -1;
    };
    TArray<FSpikeBand> Spikes;
    void AddSpikeBand(UProceduralMeshComponent* Mesh, const FVector& Floor, const FVector& Along, float HalfDepth, float HalfWidth, float Phase);
    float SpikeTip(int32 Band, double Time) const;   // the tips' height above the floor
    bool SpikeDanger(int32 Band, double Time) const { return SpikeTip(Band, Time) > SpikeDangerTip; }
    // Inside band I's footprint (with his capsule's radius).
    bool InSpikeBand(int32 Band, const FVector& Location, float Margin = 15.f) const;

    // ---- Darts.
    static constexpr float DartSpeed = 1500.f;
    static constexpr float DartClickLead = .45f;   // s between the click and the first dart
    static constexpr float DartGap = .3f;          // s between a launcher's two darts
    struct FLauncher
    {
        FVector Mouth = FVector::ZeroVector, Dir = FVector::ForwardVector;
        float Span = 360, Period = 3.2f, Phase = 0;
        float LastU = -1;
    };
    struct FDart
    {
        TWeakObjectPtr<UProceduralMeshComponent> Mesh;
        int32 Launcher = -1;
        FVector Pos = FVector::ZeroVector, Dir = FVector::ForwardVector;
        float Left = 0;
        double StuckUntil = -1;
        bool bFlying = false, bDodged = false;
    };
    TArray<FLauncher> Launchers;
    TArray<FDart> Darts;
    void AddLauncher(const FVector& Mouth, const FVector& Dir, float Span, float Period, float Phase);
    void AddDart(UProceduralMeshComponent* Mesh);
    // While a dart from launcher L could be crossing the gallery (its click to its second dart's arrival).
    bool DartLineDanger(int32 L, double Time, float Lead = 0.f) const;

    // ---- Slabs.
    static constexpr float SlabDelay = .6f;    // s after he first stands on it
    static constexpr float SlabReturn = 5.f;   // s after it fell, once he is clear
    enum class ESlab : uint8 { Whole, Cracking, Falling, Gone };
    struct FSlab
    {
        TWeakObjectPtr<UProceduralMeshComponent> Mesh;
        FVector Home = FVector::ZeroVector; FRotator HomeRotation = FRotator::ZeroRotator;
        ESlab State = ESlab::Whole; double At = 0;
        FVector Velocity = FVector::ZeroVector; FRotator Spin = FRotator::ZeroRotator;
    };
    TArray<FSlab> Slabs;
    // The slabs' pit: they come back only once he is off it (centre, axes, half extents with a margin).
    FVector RiftCentre = FVector::ZeroVector, RiftAxisA = FVector::ForwardVector, RiftAxisB = FVector::RightVector;
    float RiftHalfA = 200.f, RiftHalfB = 200.f;
    void AddSlab(UProceduralMeshComponent* Mesh);

    // ---- The trap run (2026-10-22; ChultTempleRun.h, docs/CHULT-TEMPLE-RUN.md). Each trap's
    // cycle is Period long from Phase; Back points along the run toward its start (a hit
    // knocks him that way, never sideways into a pit).
    // Great axes: a crescent blade on an arm from a pivot, swinging across the route
    // (Across) about the route's axis, Theta = Amplitude * sin(2 pi U / Period).
    struct FAxe
    {
        TWeakObjectPtr<UProceduralMeshComponent> Mesh;
        FVector Pivot = FVector::ZeroVector, Route = FVector::ForwardVector, Across = FVector::RightVector;
        float Arm = 330, Amplitude = 36, Period = 2.2f, Phase = 0;
        float LastTheta = 0;
        FRotator Base = FRotator::ZeroRotator;
    };
    TArray<FAxe> Axes;
    static constexpr float AxeBladeHalf = 55.f;    // cm along the arc either side of the arm
    static constexpr float AxeBladeIn = 30.f;    // cm inside the arm's end (the blade's back)
    static constexpr float AxeBladeOut = 42.f;   // cm outside it (the edge)
    // The crescent's back and edge radii from the pivot at U (-1..1 across the blade): widest in
    // the middle, tapering to points at the horns. The mesh and the hit test share it.
    static void AxeBladeRadii(float U, float Arm, float& OutBack, float& OutEdge)
    {
        OutEdge = Arm + 6.f + (AxeBladeOut - 6.f) * (1.f - U * U);
        OutBack = OutEdge - (3.f + (AxeBladeIn + AxeBladeOut - 6.f) * FMath::Pow(FMath::Max(0.f, 1.f - U * U), .7f));
    }
    void AddAxe(UProceduralMeshComponent* Mesh, const FVector& Pivot, const FVector& Route, float Arm, float Amplitude, float Period, float Phase);
    float AxeTheta(int32 I, double Time) const;   // degrees
    // Points on the blade (edge, middle, back) and the arm's lower half at Time (world).
    void AxePoints(int32 I, double Time, TArray<FVector>& Out) const;
    // Whether the blade meets a capsule at Location (half height, radius) at Time.
    bool AxeMeets(int32 I, double Time, const FVector& Location, float Half, float Radius) const;

    // Fire jets: a carved mouth in the wall glows and hisses (Warn), then burns for Burn,
    // a jet Reach long straight out at chest height.
    struct FJet
    {
        TWeakObjectPtr<UProceduralMeshComponent> Flame, Ember;
        TWeakObjectPtr<UPointLightComponent> Light;
        FVector Mouth = FVector::ZeroVector, Dir = FVector::ForwardVector, Route = FVector::ForwardVector;
        float Reach = 200, Period = 3.f, Phase = 0, Warn = .7f, Burn = 1.3f;
        int32 Stage = -1;
    };
    TArray<FJet> Jets;
    static constexpr float JetHalfWidth = 30.f;   // cm along the route either side of the jet's line that burns
    void AddJet(UProceduralMeshComponent* Flame, UProceduralMeshComponent* Ember, UPointLightComponent* Light, const FVector& Mouth, const FVector& Dir, const FVector& Route, float Reach, float Period, float Phase, float Warn, float Burn);
    // 0 off, 1 the warning glow, 2 burning (and how far out the flame is, 0..1).
    int32 JetStage(int32 I, double Time, float* OutExtent = nullptr) const;
    bool JetMeets(int32 I, double Time, const FVector& Location, float Half, float Radius) const;

    // Saws: discs spinning across the route (their axle along it), their lowest edge
    // at knee height; a dropping saw rises Lift above that between drops. Only a roll passes under one that is down.
    struct FSaw
    {
        TWeakObjectPtr<UProceduralMeshComponent> Disc, Arm;
        FVector Axle = FVector::ZeroVector, Route = FVector::ForwardVector, Across = FVector::RightVector;
        float Radius = 62, Lift = 0, Period = 2.4f, Phase = 0, Spin = 0;
        bool bDrops = false;
        int32 Stage = -1;
        FVector DiscBase = FVector::ZeroVector, ArmBase = FVector::ZeroVector;
    };
    TArray<FSaw> Saws;
    // A dropping saw's cycle: up (safe to run under), lowering its teeth (the tell), dropped, rising again.
    static constexpr float SawUp = .8f;     // s
    static constexpr float SawTell = .4f;
    static constexpr float SawRise = .3f;
    void AddSaw(UProceduralMeshComponent* Disc, UProceduralMeshComponent* Arm, const FVector& Axle, const FVector& Route, float Radius, bool bDrops, float Lift, float Period, float Phase);
    float SawHeight(int32 I, double Time) const;   // cm above its lowest position
    bool SawMeets(int32 I, double Time, const FVector& Location, float Half, float Radius) const;

    int32 AxeHits = 0, JetHits = 0, SawHits = 0, SawRollUnders = 0, JetsFired = 0, SawDrops = 0;

    // ---- Torches.
    void AddTorch(UPointLightComponent* Light, float Intensity);

    // Counters for the checks.
    int32 SoundsPlayed = 0;   // trap sounds actually started (only heard inside)
    int32 SpikeHits = 0, DartHits = 0, DartsFired = 0, DartDodges = 0, SlabsFallen = 0, SlabsRestored = 0, MissingSounds = 0, Thrusts = 0;
    int32 TorchCount() const { return Torches.Num(); }

private:
    struct FTorch { TWeakObjectPtr<UPointLightComponent> Light; float Intensity = 0, Phase = 0; };
    TArray<FTorch> Torches;
    double NextFlicker = 0;
    bool bHearing = false;   // Chuck is inside the interior
    TArray<USoundBase*> SpikeRattle, SpikeThrust, DartClick, DartShot, DartHit, SlabCrack, SlabFall;
    TArray<USoundBase*> AxeSwoosh, AxeCreak, FireHiss, FireRoar, SawSpin, SawDrop, TrapHit;
    TSet<int32> SawRolled;   // saws he is rolling under right now (counted once each pass)
    double NextSawSpin = 0;
    void TickAxes(AChuckCharacter* Chuck, double Now);
    void TickJets(AChuckCharacter* Chuck, double Now);
    void TickSaws(AChuckCharacter* Chuck, double Now, float Dt);
    void HitBack(AChuckCharacter* Chuck, const FVector& Route, int32& Counter, const TCHAR* What, int32 Index);
    UPROPERTY() TArray<TObjectPtr<USoundBase>> Held;
    UPROPERTY() TObjectPtr<USoundAttenuation> Attenuation;
    void LoadSounds();
    void Play(const TArray<USoundBase*>& Set, const FVector& At, float Volume);
    void TickSpikes(AChuckCharacter* Chuck, double Now);
    void TickDarts(AChuckCharacter* Chuck, double Now, float Dt);
    void TickSlabs(AChuckCharacter* Chuck, double Now, float Dt);
    void Fire(int32 L);
};
