#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FeySpores.generated.h"

class UInstancedStaticMeshComponent;

// The Feywild's spores (Claude, 2026-11-26; the user: "make puff ball and etc spore clouds look better ... make slow spores
// look better"). One actor draws every spore in the world on instanced camera-facing quads, as AChuckFlames draws fire:
//  - clouds: billows of Unity Labs' CC0 WispySmoke01 flipbook lit from within in the spore's colour
//    (M_FeySporeCloud, Tools/create_fey_spore_materials.py), swelling, rising and thinning;
//  - grains: soft glowing points that twinkle (M_FeySporeMote), flung out, slowed by the air, drifting and settling.
// Puff() is a burst (a puffball scratched or walked through, a seed head, the fairy rings' motes); AddField() is a bed whose
// spores hang over it all the time (the pollen beds that slow him): grains lifting slowly off it and a faint low haze.
// Only what is near the camera is simulated and drawn.
UCLASS()
class CHUCK3D_API AFeySpores : public AActor
{
    GENERATED_BODY()
public:
    AFeySpores();
    static AFeySpores* Get(UWorld* World);
    virtual void Tick(float DeltaSeconds) override;
    /** A burst at At (world): Grains grains flung out at up to Speed with Rise upward, and Clouds billows CloudSize across. */
    void Puff(const FVector& At, int32 Grains, float Speed, const FLinearColor& Col, float Rise, int32 Clouds, float CloudSize);
    /** One grain at At moving V (world). */
    void Grain(const FVector& At, const FVector& V, const FLinearColor& Col, float Life, float Size);
    /** A bed of spores hanging over the ground: its centre on the ground (world), half extents, yaw; gone with Owner. */
    void AddField(AActor* FieldOwner, const FVector& Centre, const FVector2D& Half, float Yaw, const FLinearColor& Col);
    static int32 Puffs;
    int32 GetGrainsDrawn() const { return GrainsDrawn; }
    int32 GetCloudsDrawn() const { return CloudsDrawn; }
    int32 GetMissingMaterials() const { return Missing; }
private:
    struct FGrain { FVector P, V; float Age = 0, Life = 1, Size = 2, Seed = 0, Buoy = 0; FLinearColor C; };
    struct FCloud { FVector P, V; float Age = 0, Life = 3, Size0 = 50, Size1 = 120, Frame = 0, Rate = 10, Spin = 0, SpinRate = 0, Peak = .5f; FLinearColor C; };
    struct FField { TWeakObjectPtr<AActor> Owner; FVector C; FVector2D Half; float Yaw = 0, Carry = 0, HazeCarry = 0; FLinearColor Col; };
    TArray<FGrain> Grains;
    TArray<FCloud> Clouds;
    TArray<FField> Fields;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> GrainFx;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> CloudFx;
    int32 GrainsDrawn = 0, CloudsDrawn = 0, Missing = 0;
    FRandomStream Rand;
    UInstancedStaticMeshComponent* Pool(const TCHAR* Name, const TCHAR* Material, int32 Count);
};
