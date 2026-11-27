#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FeyDelights.generated.h"

class UProceduralMeshComponent;
class UPointLightComponent;
class UMaterialInterface;
class UInstancedStaticMeshComponent;
class AChuckCharacter;
struct FMapSurf;

// The Feywild's small delights (Claude, 2026-11-24; docs/regions/FEYWILD.md, "Small delights"). One per loaded district, made
// by its AFeyLife and gone with it. Nothing here hurts him or changes play; each is a thing to find:
//
//  - Fairy rings: a circle of pale toadstools in a few out-of-the-way spots. Stepping inside, the music dips, a faint bell
//    sounds and motes rise round him. Nothing else.
//  - Will-o'-wisps (the Moonmoth Fen): three soft lights that drift on as he nears and settle further on, leading to a hidden
//    pool's shore with cigarettes. One light each.
//  - Koi (the Lily Mere): glowing fish under the pads of the crossing, scattering when he lands near them.
//  - Puffballs and seed heads along the roads: a scratch or a walk through bursts them in a cloud of gold spores; they grow
//    back. Harmless (the pollen's counterpart).
//  - Swans (Highmeadow): a slow line crossing below the Maze Overlook.
//  - A rainbow in the spray of the Highmeadow rapids' cascade, only where the sun is behind the viewer (M_FeyRainbow).
//  - Crows (the Deepwood): perched, turning to watch him, the odd one taking off with a call.
//  - Signposts that change when he is not looking (Highmeadow onward, where they already lie).
UCLASS()
class CHUCK3D_API AFeyDelights : public AActor
{
    GENERATED_BODY()
public:
    AFeyDelights();
    virtual void Tick(float DeltaSeconds) override;
    void Build(int32 D);
    int32 GetDistrict() const { return District; }
    bool IsBuilt() const { return Pending.IsEmpty(); }
    static const TArray<TWeakObjectPtr<AFeyDelights>>& All();
    /** His scratch (world): a puffball or seed head in reach bursts. True if one did. */
    bool TryScratch(const FVector& Location, const FVector& Ahead, float Reach);

    // Tests and stills.
    static int32 RingCigarettes;
    static int32 RingVisits; static int32 WispMoves; static int32 KoiScatters; static int32 Bursts; static int32 SignTurns; static int32 CrowFlights;
    /** The music's dip while he stands in a fairy ring (1 none; read by ADockGameMode::UpdateMusicDuck). */
    static float MusicDip;
    struct FRing { FVector C = FVector::ZeroVector; float R = 160.f; bool bGave = false; };   // (bGave: its cigarette given, 2026-11-26)
    struct FPuff { FVector P = FVector::ZeroVector; bool bSeed = false; float Gone = -1.f, Size = 1.f; };
    struct FWisp { FVector At = FVector::ZeroVector, From = FVector::ZeroVector; float Move = 1.f; };
    struct FKoi { FVector2D Home = FVector2D::ZeroVector; float R = 150.f, Phase = 0, Dir = 1, Flee = 0; FVector2D At = FVector2D::ZeroVector, Away = FVector2D::ZeroVector; float Yaw = 0; };
    struct FCrow { FVector Perch = FVector::ZeroVector; float Yaw = 0, Look = 0, Fly = -1.f, Seed = 0; FVector At = FVector::ZeroVector, Away = FVector::ZeroVector; };
    struct FSign { FVector At = FVector::ZeroVector; TArray<FVector2D> Arrows; TArray<float> Yaw; };
    const TArray<FRing>& GetRings() const { return Rings; }
    const TArray<FPuff>& GetPuffs() const { return Puffs; }
    const TArray<FWisp>& GetWisps() const { return Wisps; }
    const TArray<FVector>& GetWispWay() const { return WispWay; }
    const TArray<FKoi>& GetKoi() const { return Koi; }
    const TArray<FCrow>& GetCrows() const { return Crows; }
    const TArray<FSign>& GetSigns() const { return Signs; }
    int32 GetWispLead() const { return Lead; }
    FVector GetSwanAt(int32 I) const;
    /** Where swan I is at Time (s) into the swans' round (local). */
    static FVector SwanAt(int32 I, float Time);
    int32 GetSwanCount() const { return SwanBody.Num(); }
    FVector GetRainbow() const { return RainbowAt; }
    /** (stills) the swans held at this time into their crossing (s); < 0 runs. */
    static float SwanHold;
    /** The fairy rings' clearings, so the district's planting keeps out of them (local X, Y, radius). */
    static const TArray<FVector>& Clearings();
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    int32 District = -1, Lead = 0, InRing = -1, Respawns = -1;
    float Clock = 0, BellWait = 0, SwanClock = 0;
    bool bWasGrounded = true;
    TArray<FRing> Rings;
    UPROPERTY() TArray<AActor*> RingCigs;
    TArray<FPuff> Puffs;
    TArray<FWisp> Wisps; TArray<FVector> WispWay;
    TArray<FKoi> Koi;
    TArray<FCrow> Crows;
    TArray<FSign> Signs;
    struct FMote { FVector P = FVector::ZeroVector, V = FVector::ZeroVector; float Age = 0, Life = 1, Size = 1; FColor C = FColor::White; };
    TArray<FMote> Motes;
    void Burst(const FVector& At, int32 Count, float Speed, const FColor& Cl, float Rise);
    UProceduralMeshComponent* NewMesh(USceneComponent* Parent, const FName& Name);
    void ShowPuff(int32 I);
    void BuildRings(); void BuildPuffs(); void BuildWisps(); void BuildKoi(); void BuildSwans(); void BuildRainbow(); void BuildCrows(); void BuildSigns();
    void TickCrows(float Dt, const FVector& C, AChuckCharacter* Chuck);
    TArray<TFunction<void()>> Pending;
    UPROPERTY() TArray<UMaterialInterface*> Mats;
    UPROPERTY() TArray<UProceduralMeshComponent*> PuffMeshes;
    UPROPERTY() UProceduralMeshComponent* MoteMesh = nullptr;
    UPROPERTY() TArray<USceneComponent*> WispPivots;
    UPROPERTY() TArray<UPointLightComponent*> WispLights;
    UPROPERTY() UInstancedStaticMeshComponent* KoiMesh = nullptr;
    UPROPERTY() TArray<USceneComponent*> SwanBody;
    UPROPERTY() TArray<USceneComponent*> SwanWingL;
    UPROPERTY() TArray<USceneComponent*> SwanWingR;
    UPROPERTY() TArray<USceneComponent*> CrowBody;
    UPROPERTY() TArray<USceneComponent*> CrowWingL;
    UPROPERTY() TArray<USceneComponent*> CrowWingR;
    UPROPERTY() TArray<USceneComponent*> SignBoards;   // one pivot per arrow, at its post's top
    FVector RainbowAt = FVector::ZeroVector;
    UPROPERTY() UProceduralMeshComponent* Spray = nullptr;
    TArray<FVector> Spare;                         // the crows' free perches
    UPROPERTY() TArray<AActor*> Spawned;           // the wisps' cigarettes
    // Sounds: the fairy bell (Tools/gen_fey_delights_sfx.py), the crows' caw (same), wings, the koi's splash, the spores' puff.
    UPROPERTY() TArray<class USoundBase*> Sfx;
    UPROPERTY() class USoundAttenuation* Falloff = nullptr;
    void Play(int32 Which, const FVector& Local, float Volume = 1.f, float Pitch = 1.f);
};
