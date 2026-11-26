#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FeyCreatures.h"
#include "FeyLife.generated.h"

class UProceduralMeshComponent;
class UBoxComponent;
class UPointLightComponent;
class UMaterialInterface;
class AChuckCharacter;
class ATempleSkeleton;
class AFeyMite;
class AFeyDisplacer;

// The Feywild's mechanics and small living hazards, stage 5 part one (Claude, 2026-11-21; docs/regions/FEYWILD.md, after the
// 2D game's PHASE-9.md sections 4, 5 and 7). One per loaded district, built when the district stands and gone with it:
//
//  - Reactive flowers (2D 4): a bud on broad leaves (the goal images hit-plant-closed/open). His scratch makes it pulse, then
//    open, and only then does what it rules change: a curtain of vines draws back into the ground. Scratched again it closes
//    and the vines grow back, never while he stands in them. Back as they began when he respawns; never saved.
//  - Slowing pollen (2D 5): beds of small glowing gold flowers with spores drifting over them. On the ground in a bed he walks
//    at half speed; a jump clears a narrow strip; no damage, no lasting effect.
//  - Spitting orchids (2D 7.3): rooted, facing a lane across the way. Each swells, glows and opens, then spits a hard seed along
//    its lane on a fixed beat, staggered with its neighbours; a seed stops at what it meets and costs a Sanity point.
//  - Lantern moths (2D 7.4): wings wider than he is, lit from within, weaving back and forth over a fixed haunt; a touch costs a
//    Sanity point; they cannot be scratched.
//
// Placed: the first flower by the riverbank's road (its vines close the one way into a ring of boulders with cigarettes inside),
// pollen over the orchard's road, orchids across the Needle Garden's road, moths along the way out to the Moonmoth Fen.
// Part two (2026-11-22; FeyCreatures.h): thorn mites in the orchard and the Deepwood; the redcaps' camp in Redcap Hollow (a
// fire, a cauldron, lean-tos, three redcaps round the fire, a hollow among rocks behind a low root they cannot pass, with
// cigarettes in it); the displacer beast's round in Highmeadow across the way to the maze, with three hollow logs he can
// shelter in and pollen on the road; and at the Lily Mere a flower that raises lily pads to an islet in the deep water.
struct FFeyFlower { FVector Pos = FVector::ZeroVector; float Yaw = 0, Open = 0, Want = 0, Pulse = -1, Shown = -1; int32 Vines = -1, Pads = -1, Sc = -1; bool bHidden = false; float Grow = 1; };
/** Pads a flower raises from the deep water (local X, Y, radius), and how far up they are. */
struct FFeyPadRise { TArray<FVector> Pads; float Up = 0, Want = 0; };
struct FFeyVines { FVector C = FVector::ZeroVector; float Yaw = 0, Width = 300, Height = 190, Up = 1, Want = 1; };
struct FFeyPollen { FVector2D C = FVector2D::ZeroVector, R = FVector2D(100, 100); float Yaw = 0; };
struct FFeySeed { FVector P = FVector::ZeroVector, V = FVector::ZeroVector; float Life = 0; };
// (2026-11-23, the user: "make the plants that shoot work like the spined devils where they target chuck".) Each sleeps facing
// its lane until he is in front of it, in sight and in reach; then its bulb turns to follow him, swells, and spits at where he
// was as it began to swell (no lead), often wide. It stops when he leaves.
struct FFeyOrchid { FVector Pos = FVector::ZeroVector, Dir = FVector::ForwardVector; float Phase = 0, Range = 1500; bool bFired = false;
                    FVector Look = FVector::ZeroVector, AimAt = FVector::ZeroVector; float Beat = -1.f, Wait = 0.f; bool bAimed = false; };
struct FFeyMoth { FVector Home = FVector::ZeroVector, Axis = FVector::ForwardVector; float Range = 280, Weave = 35, Bob = 28, Rate = 1.6f, Phase = 0, TouchWait = 0; FRotator Facing = FRotator::ZeroRotator; FVector Last = FVector::ZeroVector; };

UCLASS()
class CHUCK3D_API AFeyLife : public AActor
{
    GENERATED_BODY()
public:
    AFeyLife();
    virtual void Tick(float DeltaSeconds) override;
    /** Puts in place what lives in district D (a no-op for a district with none). */
    void Build(int32 D);
    int32 GetDistrict() const { return District; }
    static const TArray<TWeakObjectPtr<AFeyLife>>& All();
    /** Tests and the route walker: no orchid or moth hurts him while set. */
    static bool bCalm;
    // (2026-11-25) The ground audit of the new beasts' rounds and roaming redcaps' leashes: samples taken, and bad ones
    // (river, deep water, steep, or something solid standing over the land on a beast's way).
    static int32 GroundSamples;
    static int32 GroundBad;
    static int32 FlowerToggles;
    static int32 OrchidShots;
    static int32 OrchidHits;
    static int32 OrchidAimed;
    /** Tests: an orchid never misses on purpose while set. */
    static bool bOrchidSure;
    static int32 MothTouches;
    // (2026-11-22, stage 6) The maze: bees knocking him off the hedge tops, statues turning while unwatched, hit plants
    // revealed the longer he is lost (first after RevealAfter seconds in the maze, then every RevealEvery).
    static int32 BeeKnocks;
    static int32 StatueTurns;
    static int32 Reveals;
    /** (2026-11-23) Thorn pricks: each second he is down among the maze's thorns (FeyMap::InMazeThorns). */
    static int32 ThornPricks;
    static float RevealAfter;
    static float RevealEvery;
    /** The next rescue plant, nearest him (local), appears now. */
    void RevealNext(const FVector& Near);
    float GetMazeTime() const { return MazeTime; }
    const TArray<float>& GetShortcutUp() const { return ScUp; }
    const TArray<FFeyFlower>& GetFlowers() const { return Flowers; }
    const TArray<FFeyVines>& GetVines() const { return Vines; }
    const TArray<FFeyPollen>& GetPollen() const { return Pollen; }
    const TArray<FFeyOrchid>& GetOrchids() const { return Orchids; }
    const TArray<FFeyMoth>& GetMoths() const { return Moths; }
    const TArray<FFeyPadRise>& GetPadRises() const { return PadRises; }
    const TArray<FFeyRefuge>& GetRefuges() const { return Refuges; }
    /** The islet (X, Y, radius) the pads lead to, the hollow behind the redcaps' camp (X, Y, radius), the camp's fire. */
    FVector GetIslet() const;
    FVector GetCache() const;
    FVector GetCamp() const;
    const TArray<AFeyMite*>& GetMites() const { return Mites; }
    const TArray<ATempleSkeleton*>& GetRedcaps() const { return Redcaps; }
    AFeyDisplacer* GetBeast() const { return Beast; }
    const TArray<class AFeyHare*>& GetHares() const { return Hares; }
    /** His scratch from Location (world) toward Ahead, within Reach: a flower in reach starts to change. */
    bool TryScratch(const FVector& Location, const FVector& Ahead, float Reach);
    /** On the ground in a pollen bed (local)? */
    bool InPollen(const FVector& Local) const;
    /** Where a moth is now (local). */
    FVector MothAt(int32 I) const;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    int32 District = -1, Respawns = -1;
    float Clock = 0;
    TArray<FFeyFlower> Flowers;
    TArray<FFeyVines> Vines;
    TArray<FFeyPollen> Pollen;
    TArray<FFeyOrchid> Orchids;
    TArray<FFeySeed> Seeds;
    TArray<FFeyMoth> Moths;
    TArray<FVector> Spores;   // (X, Y, Z) local, drifting over the pollen
    TArray<FVector> SporeBase;
    UPROPERTY() TArray<UProceduralMeshComponent*> FlowerHeads;   // petals and stamens, rebuilt as they open
    UPROPERTY() TArray<UProceduralMeshComponent*> VineMeshes;
    UPROPERTY() TArray<UBoxComponent*> VineBoxes;
    UPROPERTY() TArray<UProceduralMeshComponent*> OrchidBulbs;
    UPROPERTY() TArray<UPointLightComponent*> OrchidLights;
    UPROPERTY() TArray<USceneComponent*> MothPivots;
    UPROPERTY() TArray<USceneComponent*> MothWingL;
    UPROPERTY() TArray<USceneComponent*> MothWingR;
    UPROPERTY() UProceduralMeshComponent* SporeMesh = nullptr;
    UPROPERTY() UProceduralMeshComponent* SeedMesh = nullptr;
    UPROPERTY() TArray<UMaterialInterface*> Mats;   // petal, blade, plant, root
    UPROPERTY() TArray<AActor*> Spawned;            // its cigarettes
    UProceduralMeshComponent* NewMesh(USceneComponent* Parent, const FName& Name);
    void ShowFlower(int32 I);
    void ResetAll();
    void SpawnRedcaps();
    void BuildMaze();
    /** (2026-11-22, stage 8) What is still to be built, a part or two each frame (it was all in one frame: a 55 ms hitch). */
    TArray<TFunction<void()>> Pending;
    void TickMaze(float Dt, AChuckCharacter* Chuck, const FVector& C);
    UPROPERTY() TArray<USceneComponent*> FlowerPivots;
    UPROPERTY() TArray<USceneComponent*> ShortcutPivots;
    UPROPERTY() TArray<UBoxComponent*> ShortcutBoxes;
    UPROPERTY() TArray<USceneComponent*> Statues;
    UPROPERTY() UProceduralMeshComponent* BeeMesh = nullptr;
    UPROPERTY() class UAudioComponent* BeeAudio = nullptr;
    TArray<float> ScUp, ScWant;
    TArray<float> StatueYaw;
    float MazeTime = 0, NextReveal = 0, OnTop = 0, BeeShow = 0, NextPrick = 0;
    FVector BeeAt = FVector::ZeroVector;
    void BuildCamp();
    void BuildHag();   // (2026-11-23) the hag's cottage: its cauldron's fire and light, its chimney's smoke, their sounds
    void BuildRefuges();
    void BuildPadRise();
    TArray<FFeyPadRise> PadRises;
    TArray<FFeyRefuge> Refuges;
    float FireFlicker = 0;
    UPROPERTY() TArray<UStaticMeshComponent*> RisePadMeshes;
    UPROPERTY() TArray<UBoxComponent*> RisePadBoxes;
    UPROPERTY() TArray<AFeyMite*> Mites;
    UPROPERTY() TArray<ATempleSkeleton*> Redcaps;
    UPROPERTY() AFeyDisplacer* Beast = nullptr;
    UPROPERTY() TArray<AFeyDisplacer*> MoreBeasts;   // (2026-11-24) Highmeadow's other beasts
    UPROPERTY() TArray<class AFeyHare*> Hares;        // (2026-11-25) the great hares' herds (FeyHare.h)
    void SpawnHares();
    UPROPERTY() UPointLightComponent* FireLight = nullptr;
    UPROPERTY() UPointLightComponent* HagLight = nullptr;
    // (2026-11-22) Its sounds (Tools/build_fey_sfx.py, CC0 recordings): one-shots and the loops (moths' wings, the camp).
    UPROPERTY() TArray<class USoundBase*> Sfx;
    UPROPERTY() class USoundAttenuation* Falloff = nullptr;
    UPROPERTY() TArray<class UAudioComponent*> Loops;
    TArray<bool> OrchidSwelled;
    void Play(int32 Which, const FVector& Local, float Volume = 1.f);
    void Loop(int32 Which, USceneComponent* At, const FVector& Local, float Volume, float Radius);
};

/** His walking speed's scale on the ground at World: half in a pollen bed, else 1. */
float FeyGroundSpeedScale(const FVector& World);
/** His scratch at the Feywild's reactive flowers. True if one was in reach. */
bool FeyScratch(AChuckCharacter* Chuck, const FVector& Location, const FVector& Ahead, float Reach);
/** Circles (X, Y, radius, local) the district planting keeps clear, where a flower's nook or a bed stands. */
const TArray<FVector>& FeyLifeClearings();
