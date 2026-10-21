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
    UPROPERTY() TArray<TObjectPtr<USoundBase>> Held;
    UPROPERTY() TObjectPtr<USoundAttenuation> Attenuation;
    void LoadSounds();
    void Play(const TArray<USoundBase*>& Set, const FVector& At, float Volume);
    void TickSpikes(AChuckCharacter* Chuck, double Now);
    void TickDarts(AChuckCharacter* Chuck, double Now, float Dt);
    void TickSlabs(AChuckCharacter* Chuck, double Now, float Dt);
    void Fire(int32 L);
};
