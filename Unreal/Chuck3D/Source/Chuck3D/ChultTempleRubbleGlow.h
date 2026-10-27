#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChultTempleRubbleGlow.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;

/**
 * The rubble crevice's glow (Claude, 2026-10-26; user: "have it glow faintly gold like golden leaf ciggeretes so that the
 * players attention is drawn"): the golden cigarette's aura (M_GoldenLeafAura, CigarettePickup.cpp) at the crawl's mouth,
 * fainter and wider: a low haze in the gap, a faint wisp rising off the pile, motes drifting up out of it, a warm light
 * breathing on its stones and a little way into the passage. Placed by the rubble map's builder; no gameplay.
 */
UCLASS()
class CHUCK3D_API AChultTempleRubbleGlow : public AActor
{
    GENERATED_BODY()
public:
    AChultTempleRubbleGlow();
    virtual void Tick(float DeltaSeconds) override;
    /** At: the mouth's floor (world); Out: out of the gap into the chamber (world, level). */
    void Setup(const FVector& At, const FVector& Out);
    static constexpr int32 MoteCount = 14;
private:
    UPROPERTY() UStaticMesh* Sphere = nullptr;
    UPROPERTY() UStaticMesh* MoteMesh = nullptr;
    UPROPERTY() UStaticMeshComponent* Haze = nullptr;
    UPROPERTY() UStaticMeshComponent* Wisp = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* Motes = nullptr;
    UPROPERTY() UPointLightComponent* Light = nullptr;
    UPROPERTY() UPointLightComponent* Inner = nullptr;
    UPROPERTY() UMaterialInstanceDynamic* HazeMat = nullptr;
    UPROPERTY() UMaterialInstanceDynamic* WispMat = nullptr;
    UPROPERTY() UMaterialInstanceDynamic* MoteMat = nullptr;
    FVector Base = FVector::ZeroVector, Ahead = FVector::ForwardVector;
    float Clock = 0;
};
