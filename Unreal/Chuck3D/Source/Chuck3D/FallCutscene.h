#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FallCutscene.generated.h"

class AChuckCharacter;
class ACameraActor;
class UAudioComponent;
class USoundBase;
class UStaticMesh;
class UInstancedStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;

/**
 * Phase 3's end (user 2026-10-12): Chuck falls through the cellar's sky
 * hole, through a long open sky and the cloud deck, crashes down through the
 * Chult canopy, dies his quiet astral death on the jungle floor, is summoned
 * back, looks round and takes a drag. Authored after the 2D game's
 * falling_cutscene_scene.py, in the engine's own 3D, and cut to the user's
 * "Fall Cutscene theme" (SW_FallToChult): its first 47.5 s, the opening two
 * sections, faded out over the last two bars of the second (43.8-47.5 s).
 * The clock is song time; bar lines at 0.55 + 1.879 k s (128 bpm).
 *
 * No player control: Esc skips to the end. The jungle here is the cutscene's
 * own set (Tools/build_fall_jungle.py) on a stage far from the docks. Completion
 * and Esc-skip both restore gameplay in the matching Chult landing setting.
 */
UCLASS()
class CHUCK3D_API AFallCutscene : public AActor
{
    GENERATED_BODY()
public:
    AFallCutscene();
    virtual void Tick(float DeltaSeconds) override;
    static AFallCutscene* Start(AChuckCharacter* Chuck);
    /** Esc: a short fade to the end. */
    void Skip();
    float GetClock() const { return Clock; }
    bool IsMusicPlaying() const;
    // Song-time beats of the sequence.
    static constexpr float Lead = 5.f;           // s of falling with only the air before the score (clock starts at -Lead)
    static constexpr float Switch = -3.8f;       // white: the pantry gives way to open sky
    static constexpr float Reveal = .55f;        // the score's first hit: the wide sky
    static constexpr float CloseUp = 8.07f;      // the score lifts: his face
    static constexpr float Down = 13.70f;        // looking down past him at the cloud deck
    static constexpr float Below = 17.46f;       // in the cloud, out under it over the jungle
    static constexpr float Canopy = 21.22f;      // from the canopy, looking up at him coming
    static constexpr float Crash = 24.98f;       // the breakdown: into the leaves
    static constexpr float Impact = 26.30f;
    static constexpr float Summon = 28.73f;      // the light gathers round him where he lies
    static constexpr float SwapLead = .11f;      // s before the column's peak that it takes him
    static constexpr float ProneYaw = 70.f;      // lying across the floor camera's view
    static constexpr float ProneHeight = 14.f;   // capsule centre above the floor when flat on his front
    static constexpr float ShakeTime = .6f;
    static constexpr float Look = 32.49f;
    static constexpr float Drag = 36.25f;
    static constexpr float PullBack = 40.01f;
    static constexpr float MusicFade = 43.76f;
    static constexpr float Black = 45.64f;
    static constexpr float End = 47.52f;
    /** The landing spot on the stage (the jungle floor). */
    static FVector LandingSpot();
    /** The cloud deck (the engine material's Cloud_GlobalCoverage and Cloud_GlobalDensity). */
    static constexpr float CloudCoverage = .22f;
    static constexpr float CloudDensity = .035f;
private:
    TWeakObjectPtr<AChuckCharacter> Chuck;
    UPROPERTY() ACameraActor* Camera;
    UPROPERTY() UAudioComponent* Music;
    UPROPERTY() UAudioComponent* Wind;
    UPROPERTY() UAudioComponent* Flap;   // his jacket in the rushing air
    UPROPERTY() UAudioComponent* Whistle;   // high air on the fastest stretches
    UPROPERTY() USoundBase* ThudSound;      // his body hitting the jungle floor
    UPROPERTY() TArray<USceneComponent*> SkyParts;
    UPROPERTY() class AChultLanding* Landing;
    UPROPERTY() TArray<USoundBase*> LeafCrashes;
    UPROPERTY() USoundBase* LandSound;
    UPROPERTY() UDirectionalLightComponent* Sun;
    UPROPERTY() USkyLightComponent* SkyLight;
    UPROPERTY() UExponentialHeightFogComponent* Fog;
    UPROPERTY() UInstancedStaticMeshComponent* FallingLeaves;
    UPROPERTY() TArray<UHierarchicalInstancedStaticMeshComponent*> Jungle;
    UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Puffs;
    float Clock = -Lead;
    float SkipAt = 0;
    bool bSkipping = false;
    bool bStage = false, bLanded = false, bVanished = false, bSummoned = false, bFinished = false;
    int32 Exhales = 0, Crashes = 0;
    FVector PantryStart = FVector::ZeroVector;
    float PantryYaw = 0;
    struct FLeaf { FVector Position; FVector Velocity; FRotator Rotation; FRotator Spin; float Age; float Life; };
    TArray<FLeaf> Leaves;
    float ShakeUntil = -1e9f;   // (the clock starts negative: no shake until set)
    int32 PartFrames[3] = {0, 0, 0};
    float PartTime[3] = {0, 0, 0}, PartWorst[3] = {0, 0, 0};
    void BuildStage();
    void HideDocks();
    void Burst(const FVector& At, int32 Count, float Speed);
    void UpdateLeaves(float DeltaSeconds);
    void UpdateChuck(float DeltaSeconds);
    void UpdateCamera();
    void UpdateLight();
    void Events(float From, float To);
    void Finish();
    /** Height of his feet above the jungle floor during the descent (cm). */
    static float Altitude(float T);
    FVector ChuckTarget() const;
    void Aim(const FVector& From, const FVector& At, float FocalMM, float FStop);
};
