#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FeyLife.generated.h"

class UProceduralMeshComponent;
class UBoxComponent;
class UPointLightComponent;
class UMaterialInterface;
class AChuckCharacter;

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
// Thorn mites, redcaps, the displacer beast and the flowers that raise lily pads are part two.
struct FFeyFlower { FVector Pos = FVector::ZeroVector; float Yaw = 0, Open = 0, Want = 0, Pulse = -1, Shown = -1; int32 Vines = -1; };
struct FFeyVines { FVector C = FVector::ZeroVector; float Yaw = 0, Width = 300, Height = 190, Up = 1, Want = 1; };
struct FFeyPollen { FVector2D C = FVector2D::ZeroVector, R = FVector2D(100, 100); float Yaw = 0; };
struct FFeySeed { FVector P = FVector::ZeroVector, V = FVector::ZeroVector; float Life = 0; };
struct FFeyOrchid { FVector Pos = FVector::ZeroVector, Dir = FVector::ForwardVector; float Phase = 0, Range = 1500; bool bFired = false; };
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
    static int32 FlowerToggles;
    static int32 OrchidShots;
    static int32 OrchidHits;
    static int32 MothTouches;
    const TArray<FFeyFlower>& GetFlowers() const { return Flowers; }
    const TArray<FFeyVines>& GetVines() const { return Vines; }
    const TArray<FFeyPollen>& GetPollen() const { return Pollen; }
    const TArray<FFeyOrchid>& GetOrchids() const { return Orchids; }
    const TArray<FFeyMoth>& GetMoths() const { return Moths; }
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
};

/** His walking speed's scale on the ground at World: half in a pollen bed, else 1. */
float FeyGroundSpeedScale(const FVector& World);
/** His scratch at the Feywild's reactive flowers. True if one was in reach. */
bool FeyScratch(AChuckCharacter* Chuck, const FVector& Location, const FVector& Ahead, float Reach);
/** Circles (X, Y, radius, local) the district planting keeps clear, where a flower's nook or a bed stands. */
const TArray<FVector>& FeyLifeClearings();
